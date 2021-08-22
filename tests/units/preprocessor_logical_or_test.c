#define ABC 50 || 30
#if(ABC || 90)
#define ABC 1
#endif

int main()
{
    return ABC;
}