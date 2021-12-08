#include <stdio.h>
#include <stdlib.h>
#define BOOK_NAME_SIZE 20
struct book
{
    char name[BOOK_NAME_SIZE];
    int year;
};

int main() {
    int num;
    printf("How many books do you want to process: ");
    scanf("%d", &num);

    struct book* books;
    size_t book_size = sizeof(struct book);
    books = calloc(book_size, num);

    int i = 0;
    for (i = 0; i < num; i++)
    {
        char* name_ptr;
        name_ptr =  &books[i].name;
        //scanf("%s", &books[i].name);
       printf("%s", name_ptr);
    }
    return 0;
}