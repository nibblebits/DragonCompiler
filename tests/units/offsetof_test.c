#include <stddef.h>
#include <stdio.h>

struct address {
   char name[50];
   char street[50];
   int phone;
};
   
int main () {
   int name_offset = offsetof(struct address, name);
   int street_offset = offsetof(struct address, street);
   int phone_offset = offsetof(struct address, phone);

   return name_offset + street_offset + phone_offset;
} 