struct abc
{
    int a;
    char b;
    int c;
};


struct abc d;
int main()
{
    struct abc a;
    a.a = 50;
    a.b = 20;
    a.c = 30;

    d.a = 1;
    d.b = 2;
    d.c = 3;

    return a.a + a.b + a.c + d.c + d.b + d.a;
}