#ifndef TRAPPER_H
#define TRAPPER_H

#define __USE_GNU
#include <signal.h>

int inspector_inspect(char* code, int code_len, mcontext_t* context_before, mcontext_t* context_after);
void inspector_init();

#endif /* TRAPPER_H */

