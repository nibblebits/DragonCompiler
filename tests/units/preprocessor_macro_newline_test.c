#define ABC 50
#if ABC == 50 && \
ABC == 50
#define ABC 1
#endif


int main()
{
    return ABC;
}