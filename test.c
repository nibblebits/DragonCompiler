struct cat
{
    int bb;
    int ee;
};
struct abc
{
    int x;
    int y;
    struct cat c;
};

struct abc abc1;
struct abc abc2;

struct abc test()
{
    return abc1;
}

int main()
{
   test();
}