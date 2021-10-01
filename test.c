#include <stdarg.h>

// We will need to automate the building of the prototype.
// For now it is not added to AST automatically.
int va_start(__builtin_va_list list, void *x);
void va_copy(__builtin_va_list list, __builtin_va_list list2);

void va_end(__builtin_va_list list);

__ignore_typecheck__ void *__builtin_va_arg(__builtin_va_list list, int elem_size);
#define va_arg(list, type) __builtin_va_arg(list, sizeof(type))


int printf(const char *s, ...);

int omg(int x, ...)
{
    __builtin_va_list list;
        __builtin_va_list list2;

    va_start(list, x);

    int y;
    int i;
    va_copy(list, list2);

    va_end(list);
}

int main(int x, ...)
{
    omg(2, 30, 20);
}
