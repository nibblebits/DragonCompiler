
struct dd
{
    int e;
    int y;
    int b;
    int z[20][10];
};
struct dog
{
    int x;
    struct dd* d;
};
int main()
{
    int e;
    e = 5;

    int m;
    m = 3;
    struct dd o;
    struct dog d;
    d.d = &o;
    o.z[m][5] = 25;

    return d.d->z[3][e];

}   