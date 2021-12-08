#include <stdio.h>
int main()
{
    int num;
    printf("Enter a number: ");
    scanf("%i", &num);
    printf("Number %i\n", num);
    if (num < 0)
    {
        printf("You entered a negative number.");
    }
    else if (num == 0)
    {
        printf("YOu entered zero\n");
    }
    else
    {
        printf("You entered a positive number.");
    }
    return 0;
}
