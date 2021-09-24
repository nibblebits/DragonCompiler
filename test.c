struct abc
{
    char dd;
    char ee;
    int y;
};

int oi(int b, struct abc a, int cc)
{
    a.dd = 20;
    a.ee = 40;
    a.y = 40;
}
int main()
{
    struct abc d;
    oi(50, d, 20);
}