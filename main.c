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
#include <ctype.h>

#include "inspector.h"

static void show_differences(mcontext_t* before, mcontext_t* after)
{
	static char* reg_names[] = {
		"REG_R8",  "REG_R9",  "REG_R10", "REG_R11", "REG_R12", "REG_R13", "REG_R14", "REG_R15",
		"REG_RDI", "REG_RSI", "REG_RBP", "REG_RBX", "REG_RDX", "REG_RAX", "REG_RCX", "REG_RSP",
		"REG_RIP", "REG_EFL", "REG_CSGSFS", "REG_ERR", "REG_TRAPNO", "REG_OLDMASK", "REG_CR2"
	};

	for (int i = 0; i < __NGREG; i++)
	{
		uint64_t value_before = before->gregs[i];
		uint64_t value_after = after->gregs[i];
		if (i != REG_TRAPNO && value_before != value_after)
		{
			printf("%s: 0x%016lx -> 0x%016lx\n",
					reg_names[i], value_before, value_after);
		}
	}
}

static int hex_to_bin(char* hex, unsigned char* data, int maxlen)
{
	int len = 0;
	int nibpos = 0;
	unsigned char nibble = 0;

	while (*hex)
	{
		char c = tolower(*hex++);

		if (c == ' ')
		{
			continue;
		}

		nibble <<= 4;

		if (c >= '0' && c <= '9')
		{
			nibble |= c - '0';
		}
		else if (c >= 'a' && c <= 'f')
		{
			nibble |= c - 'a' + 10;
		}
		else
		{
			fprintf(stderr, "Invalid hex character: '%c'\n", c);
			return -1;
		}

		if (++nibpos == 2)
		{
			if (len >= maxlen)
			{
				fprintf(stderr, "Hex string too long\n");
				return -1;
			}
			data[len++] = nibble;
			nibpos = 0;
		}
	}

	if (nibpos != 0)
	{
		fprintf(stderr, "Hex string has an odd number of nibbles\n");
		return -1;
	}

	return len;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Need a hex string representing code (15 bytes max) as argument.\n");

		return 1;
	}

	/* Read the data as a hex string */
	unsigned char code[15];
	int data_size = hex_to_bin(argv[1], code, sizeof(code));
	if (data_size == -1)
	{
		return 1;
	}

	inspector_init();

	mcontext_t before, after;
	int len = inspector_inspect(code, sizeof(code), &before, &after);
	if (len == -1)
	{
		fprintf(stderr, "Failed to inspect code\n");
		return 1;
	}

	printf("Instructrion size: %d\n", len);
	printf("Full instruction (hex): ");
	for(int i = 0; i < len; i++)
	{
		printf("%02x ", code[i]);
	}
	printf("\n");
	show_differences(&before, &after);

	return 0;
}

