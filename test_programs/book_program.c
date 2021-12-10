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
    int i;
    for (i = 0; i < num; i+=1)
    {
        printf("Enter a book name\n");
        scanf("%s", &books[i].name);
        printf("Enter a year:\n");
        scanf("%i", &books[i].year);
    }

    for (i=0; i < num; i+=1)
    {
        printf("Book: %s published %i\n", &books[i].name, books[i].year);
    }
    return 0;
}