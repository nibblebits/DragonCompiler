#define CBA 22
#define ABC 50
#undef ABC
#ifdef ABC
#define CBA 10
#endif

int main()
{
   return CBA;
}