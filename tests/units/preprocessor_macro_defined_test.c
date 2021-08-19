
#define ABC
#define CBA 0
#if defined ABC && 21 > 20
#define CBA 10
#endif
int main()
{
    return CBA;
}