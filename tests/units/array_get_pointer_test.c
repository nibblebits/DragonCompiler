
#define BOOK_NAME_SIZE 20
struct book
{
    char name[BOOK_NAME_SIZE];
    int year;
};

struct book b[2];
int main() {

    b[1].year = 20;
    struct book* books;
    books = &b;
    struct book* book = &books[1];
    
    return book->year;
}