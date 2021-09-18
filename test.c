#include <stddef.h>

int main()
{
   size_t x;
   x = 15 * sizeof(int) - 4 * sizeof(void *) - sizeof (size_t);
}