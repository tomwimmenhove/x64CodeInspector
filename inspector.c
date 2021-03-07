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

/* I don't think there's a way to avoid globals, since signal handlers
 * don't have any non-gblobal context by definition */
static __thread char* trap_address;
static __thread int trap_state;
static __thread char original_opcode;
static __thread jmp_buf trap_jmp_buf;
static __thread mcontext_t* trap_context_before;
static __thread mcontext_t* trap_context_after;
static __thread int trap_failed = 0;

enum
{
	TRAP_STATE_INT3,
	TRAP_STATE_TRAP,
};

static void run_code_at(void *address) { ((void (*)()) address)(); }

static int trap_code_at(void *address, mcontext_t* context_before, mcontext_t* context_after)
{
	trap_context_before = context_before;
	trap_context_after = context_after;
	trap_failed = 0;

	if (sigsetjmp(trap_jmp_buf, 1) == 0)
	{
		trap_address = (char*) address;

		original_opcode = *trap_address;
		*trap_address = 0xcc; // INT3 opcode

		trap_state = TRAP_STATE_INT3;

		run_code_at(address);
	}

	return trap_failed;
}

static void trap_handler(int signum, siginfo_t* siginfo, void* context)
{
	ucontext_t* ctx = (ucontext_t*) context;

	if (signum == SIGSEGV)
	{
		ctx->uc_mcontext.gregs[REG_EFL] &= ~0x100;
		trap_failed = 1;
		siglongjmp(trap_jmp_buf, 1);

		return;
	}

//	printf("Trapped @0x%016lx\n", (uint64_t) ctx->uc_mcontext.gregs[REG_RIP]);

	switch(trap_state)
	{
		case TRAP_STATE_INT3:
			/* Execute the same instruction again */
			ctx->uc_mcontext.gregs[REG_RIP]--;

			/* Save the 'before' state */
			memcpy(trap_context_before, &ctx->uc_mcontext, sizeof(mcontext_t));

			/* Set the trap flag */
			ctx->uc_mcontext.gregs[REG_EFL] |= 0x100;

			/* Restore the original opcode */
			*trap_address = original_opcode;

			trap_state = TRAP_STATE_TRAP;

			break;
		case TRAP_STATE_TRAP:
			/* unset the trap flag */
			ctx->uc_mcontext.gregs[REG_EFL] &= ~0x100;

			/* Save the 'after' state */
			memcpy(trap_context_after, &ctx->uc_mcontext, sizeof(mcontext_t));

			/* Pretend nothing hacodeened */
			siglongjmp(trap_jmp_buf, 1);

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

