#include <stdio.h>
#include <stdlib.h>
#define BOOK_NAME_SIZE 20
struct book
{
    char name[BOOK_NAME_SIZE];
};
struct book b[2];
int main() {

    printf("%s", b[1].name);

 
}