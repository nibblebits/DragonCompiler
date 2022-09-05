int printf(const char* s, ...);

int special(int x, int y)
{
    return x * y;
}
int main()
{
    printf("%i\n", special(50, 20));
}