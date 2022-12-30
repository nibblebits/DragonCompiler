#include <stdio.h>


#define TEST_FUNC(x) #x
int main()
{
    const char* s = TEST_FUNC(hello world);

    printf("%s", s);
    // printf("%s", TEST_FUNC((hello world)));
}