#define TEST_FUNC(x) #x

int memcmp(const void *str1, const void *str2, int n);
int main()
{
     if(memcmp("hello", TEST_FUNC(hello), 5) == 0)
     {
        return 1;
     }

     return 0;
}