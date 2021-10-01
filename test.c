#include <stdarg.h>

int printf(const char *s, ...);

int omg(int x, ...)
{
    int list;
    va_start(list, x);

    int y;
    int i;
    for (i = 0; i < x; i = i + 1)
    {
        y = va_arg(list, int);
        printf("%i\n", y);
    }

    va_end(list);
}

int main(int x, ...)
{
    omg(2, 30, 20);
}
