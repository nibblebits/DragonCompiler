#include <stdio.h>
int printf(const char* c, ...);
int main()
{  
   int x = 1;
   x = x << 1;
   printf("%i\n", x);
}