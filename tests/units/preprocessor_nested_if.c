
#define ABC 50
#ifdef ABC
    #define CBA 2
    #ifdef CBA
    #if CBA > 1
    #define CBA		1
    #define AAA 22
    #endif
    #else
    #define AAA 15
    #endif
#endif

#ifdef CCC
#define AAA 10
#endif

int main()
{
    int x;
    x = AAA;
    return x;
}

