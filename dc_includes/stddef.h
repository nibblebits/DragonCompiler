#ifndef STDDEF_H
#define STDDEF_H

typedef int __builtin_va_list;
typedef __builtin_va_list va_list;

#include <stddef-internal.h>

#define offsetof(TYPE, MEMBER) &((TYPE*)0x00)->MEMBER
#define va_arg(list, type) __builtin_va_arg(list, sizeof(type))


#endif