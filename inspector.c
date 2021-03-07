/* 
 * This file is part of the x64CodeInspector distribution (https://github.com/tomwimmenhove/x64CodeInspector).
 * Copyright (c) 2021 Tom Wimmenhove.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <setjmp.h>
#include <string.h>

#include "inspector.h"

enum
{
	/* Initial state. Used to save the 'before' processor state
	 * and to set the trap flag. */
	TRAP_STATE_INT3,

	/* State we're in after the instruction in question has been
	 * executed, so we can save the 'after' processor state. */
	TRAP_STATE_TRAP,
};

struct inspector_state
{
	char*		address;		// Address of the code to be executed
	int		state;			// State in the trap handler
	char		original_opcode;	// Original first byte of the code
	jmp_buf		saved_state;		// Saved state before execution
	mcontext_t*	context_before;		// Processor state before execution
	mcontext_t*	context_after;		// Processor state after execution
	int		failed;			// Execution failed (SIGSEGV)
};

/* I don't think there's a way to avoid globals, since signal handlers
 * don't have any non-gblobal context by definition */
static __thread struct inspector_state inspector_ctx;

static void run_code_at(void *address) { ((void (*)()) address)(); }

static int trap_code_at(void *address, mcontext_t* context_before, mcontext_t* context_after)
{
	/* Initialize global context */
	inspector_ctx.failed		= 0;
	inspector_ctx.state		= TRAP_STATE_INT3;
	inspector_ctx.context_before	= context_before;
	inspector_ctx.context_after	= context_after;
	inspector_ctx.address		= (char*) address;
	inspector_ctx.original_opcode	= *inspector_ctx.address;
	
	/* Replace the first byte of the code with 0xcc (INT3 opcode), forcing the
	 * trap handler to be called when the code is executed */
	*inspector_ctx.address = 0xcc;

	if (sigsetjmp(inspector_ctx.saved_state, 1) == 0)
	{
		run_code_at(address);
	}

	return inspector_ctx.failed;
}

static void trap_handler(int signum, siginfo_t* siginfo, void* data)
{
	ucontext_t* trap_ctx = (ucontext_t*) data;

//	printf("Signal %d @0x%016lx\n", signum, (uint64_t) ctx->uc_mcontext.gregs[REG_RIP]);

	/* SIGSEGV causes trap_code_at() to return an error */
	if (signum == SIGSEGV)
	{
		/* Reset the trap flag */
		trap_ctx->uc_mcontext.gregs[REG_EFL] &= ~0x100;

		/* Set the return value for trap_code_at() */
		inspector_ctx.failed = -1;

		/* Restore state and make sigsetjmp() in trap_code_at() return non-zero */
		siglongjmp(inspector_ctx.saved_state, 1);

		return;
	}

	switch(inspector_ctx.state)
	{
		case TRAP_STATE_INT3:
			/* Execute the same instruction again */
			trap_ctx->uc_mcontext.gregs[REG_RIP]--;

			/* Save the 'before' state */
			memcpy(inspector_ctx.context_before, &trap_ctx->uc_mcontext, sizeof(mcontext_t));

			/* Set the trap flag */
			trap_ctx->uc_mcontext.gregs[REG_EFL] |= 0x100;

			/* Restore the original opcode */
			*inspector_ctx.address = inspector_ctx.original_opcode;

			/* After execution, fall into the next state */
			inspector_ctx.state = TRAP_STATE_TRAP;

			break;
		case TRAP_STATE_TRAP:
			/* Reset the trap flag */
			trap_ctx->uc_mcontext.gregs[REG_EFL] &= ~0x100;

			/* Save the 'after' state */
			memcpy(inspector_ctx.context_after, &trap_ctx->uc_mcontext, sizeof(mcontext_t));

			/* Restore state and make sigsetjmp() in trap_code_at() return non-zero */
			siglongjmp(inspector_ctx.saved_state, 1);

			break;
		default:
			fprintf(stderr, "Something went wrong\n");
			exit(1);
	}
}

void inspector_init()
{
	struct sigaction action_handler;
	action_handler.sa_flags = SA_SIGINFO;
	action_handler.sa_sigaction = trap_handler;
	sigaction(SIGTRAP, &action_handler, NULL);
	sigaction(SIGSEGV, &action_handler, NULL);
}

int inspector_inspect(char* code, int code_len, mcontext_t* context_before, mcontext_t* context_after)
{
	int page_size = getpagesize();

	/* Allocate 2 pages and allow execution on te first page. This way
	 * we can find the size of the isntruction by placing it on the boundary
	 * of the executable and non-executable page, and shift it left until it
	 * doesn't segfault */
	char *p = mmap(0, page_size * 2, PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	mprotect(p, getpagesize(), PROT_WRITE | PROT_READ | PROT_EXEC);

	/* Max instruction size is 15 bytes */
	for (int i = 1; i < 15; i++)
	{
		char *code_at = p + page_size - i;
		memcpy(code_at, code, code_len);

		if (trap_code_at(code_at, context_before, context_after) == 0)
		{
			return i;
		}
	}

	return -1;
}

