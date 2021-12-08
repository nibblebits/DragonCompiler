
#include <stdio.h>
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("You must provide at least one argument\n");
        return -1;
    }

    int i;
    for (i = 1; i < argc; i+=1)
    {
        printf("Argument %s\n", argv[i]);
    }

}   
