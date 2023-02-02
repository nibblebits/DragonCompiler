#include <stdarg.h>
#include <stddef.h>

int main()
{
    va_list abc;
    int num_args = 4;
    va_start(abc, num_args);
    return 22;
}

