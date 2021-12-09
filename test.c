
#define BOOK_NAME_SIZE 20
struct book
{
    char name[BOOK_NAME_SIZE];
    int year;
};

int test(char* fmt, char* s, int a)
{
    return a;
}
struct book book;
int main() {
    struct book* books;
    return test("%s published %i\n", books[0].name, 2000);
}