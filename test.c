#define ABC_SUM(x, y) x + y

#define ABC ABC_SUM(5, 5)
#if ABC
    int x = 50;
#endif

int main()
{
    return x;
}