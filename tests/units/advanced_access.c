struct ee
{
    int d;
    int e[50][20];
};

struct dog
{
   struct ee* a;
};

int main()
{
    struct dog d;
    struct ee o;
    d.a = &o;

    int m;
    m = 12;
    int e;
    e = 10;

    d.a->e[10][12] = 50;
    int dd;
    dd = d.a->e[10][12] - d.a->e[e][m] + d.a->e[10][m] - d.a->e[e][12] + d.a->e[e][m];
    return dd;
}   