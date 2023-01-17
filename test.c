#include <stddef.h>
#include <stdio.h>

struct address {
   char m;
   char d;
   int mm;
   char c;
};

struct address a;

int main () {
   int pos = offsetof(struct address, c);

   return 0;
} 