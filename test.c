char* abc[4];
int printf(const char* s, ...);
int main()
{
    abc[0] = "hello";
    abc[1] = "world";

    char c;
    c = abc[0][1];
    printf("%c", c);
}