#ifndef TRAPPER_H
#define TRAPPER_H

#define __USE_GNU
#include <signal.h>

int inspect_instruction(char* code, int code_len, mcontext_t* context_before, mcontext_t* context_after);
void init_trap();

#endif /* TRAPPER_H */

