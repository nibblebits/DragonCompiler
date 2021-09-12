
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
   return TEST;
}
