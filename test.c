#include <stdio.h>


int strlen(const char* s);
int main()
{
  FILE* file;
  file =fopen("./test.txt", "w");
  int len;
  fwrite("hello", strlen("hello"), 1, file);

}