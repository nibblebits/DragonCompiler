#define ABC_SUM(x, y) x+y
#define ABC ABC_SUM(5, 10)
#if ABC == 15
int x = 50;
#else
int x = 20;
#endif

int main()
{
    return x;
}
