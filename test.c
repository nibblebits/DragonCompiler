
int e = 10;
int z = 20;

struct ant
{
    int m;
    int e;
};

struct elf
{
    int k;
    int z;
    struct ant* ii;
};

struct dog
{
    int a;
    int k;
    struct elf* b;
};

struct dog* d;
int main()
{
    z = d[1].k;

}