struct wolf
{
    int e;
    int d;
};
struct dog
{
    int x;
    int y;
    struct wolf d;
};

struct dog d;
struct dog* abc(int x)
{
    d.y = 50;
    d.d.d = 75;
    return &d;
}

int main()
{
    return abc(5)->d.d;
}