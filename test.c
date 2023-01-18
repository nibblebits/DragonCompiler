#include <stddef.h>
#include <stdio.h>

struct doggy
{
   int a;
   int b;
};
struct abc
{
   int a;
   int b;
   struct doggy d;

};

struct address
{
   struct abc kk;
};

struct address a;

int main()
{
   int pos = offsetof(struct address, kk.d.b);

   return pos;
}