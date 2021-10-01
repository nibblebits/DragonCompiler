#ifndef STDARG_H
#define STDARG_H
#include <stdarg-internal.h>

int va_start(__builtin_va_list list, void *x);
void va_copy(__builtin_va_list list, __builtin_va_list list2);

void va_end(__builtin_va_list list);

__ignore_typecheck__ void *__builtin_va_arg(__builtin_va_list list, int elem_size);
#define va_arg(list, type) __builtin_va_arg(list, sizeof(type))

#endif