#define TEST(x, y, z) x(y, z)
#define ABC TEST(abc, 50, 20)
int abc(int xx, int dd)
{
    return xx+dd;
}

int main()
{
    ABC;
}