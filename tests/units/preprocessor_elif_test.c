#define DDD 10
#define CBA 60
#define ABC

#if 0
int x = 50;
#elif CBA > 90
int x = 20;
#elif CBA > 40
int x = 90;
#else
int x = 40;
#endif



#define __WORDSIZE 32
#if __WORDSIZE == 32
#define TEST 50

#elif 0+0
#define TEST 90
#else
#define TEST 40
#endif

int main()
{
    return x;
}