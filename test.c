#include <stdio.h>

int test(int x)
{
  return x;
}

int bb(int x, int y)
{
  return x+y;
}
int printf(const char* s, ...);
int main()
{
  int i;

  printf("%i\n", bb(test(50)+10, test(15)));

}