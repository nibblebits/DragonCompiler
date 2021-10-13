#include <stdio.h>

int printf(const char* s, ...);
int main()
{
  int i;
  i = 50;
  int* ptr;
  ptr = &i;

  int b;
  b = *ptr;
  printf("%i\n", b);

}