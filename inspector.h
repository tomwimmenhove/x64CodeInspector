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

#ifndef TRAPPER_H
#define TRAPPER_H

#define __USE_GNU
#include <signal.h>

int inspector_inspect(char* code, int code_len, mcontext_t* context_before, mcontext_t* context_after);
void inspector_init();

#endif /* TRAPPER_H */

