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

int main()
{
    abc2.x = 56;
    abc2.y = 20;
    abc1 = abc2;

    return abc2.x + abc2.y;
}