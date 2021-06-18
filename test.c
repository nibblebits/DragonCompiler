
int e = 10;
int z = 20;

struct ant
{
    int m;
    int e;
};

struct elf
{
    char m;
    char e;
};

struct dog
{
    int a;
    int k;
    struct elf b;
};

struct dog d[6];
int main()
{
    z = d[0].k;

}