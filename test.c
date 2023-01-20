#include <stddef.h>
#include <stdio.h>

struct address {
   int god;
   int phone;
   char e;
};
   
int main () {
   int a; 
   int sizeof_offset = sizeof(a);
   return sizeof_offset;
} 