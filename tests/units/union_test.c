union mm
{
   int e;
   int k;
};

struct dog
{
   int x;
   int m;
   union mm aa;
};

struct dog d;
int main()
{
   d.x = 10;
   d.m = 20;
   d.aa.k = 50;

   // Should be 50.
   return d.aa.e;
}