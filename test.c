
struct cat
{
    int x;
    int* y;
};
struct dog
{
    int e;
    int d;
    struct cat* s;
};

struct dog d;
struct cat c;
int main()
{
    int a;
    a = 50;
    c.y = &a;
    d.s = &c;
    return *d.s->y;
}