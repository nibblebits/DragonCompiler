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
    int dd;
    dd = d.a->e[10][12];
    return dd;
}   