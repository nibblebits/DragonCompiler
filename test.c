
int e = 10;
int z = 20;

struct ant
{
    int m;
    int e;
};

struct elf
{
    char e;
    char kk;
};

struct dog
{
    int k;
    int e;
    int z;
    struct elf* b;

};

struct dog d[4];
int main()
{
    z = d[1].b->kk;

}