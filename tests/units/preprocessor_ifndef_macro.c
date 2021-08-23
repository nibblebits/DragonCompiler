#define ABC 56
#ifndef CAT
#define ABC 10
#endif

#define OOO 10
#ifndef OOO
#define OOO 5
#else
#define OOO 7
#endif

int main()
{
    return ABC + OOO;
}