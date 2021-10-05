

#define ABC(A, F) A ## F
int abc(int x, int y)
{
    return x+y;
}
int main()
{
   int ABC(ABC, 55);
   ABC55 = 10;
   return 0;
}