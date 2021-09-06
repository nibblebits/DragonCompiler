#define ABC_SUM(x, y) x+y

#if ABC_SUM(5, 10) == 15
int x = 50;
#else
int x = 20;
#endif

int main()
{
    return x;
}