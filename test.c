struct mm
{
   int e;
   int k;
};

struct dog
{
   struct mm aa;
};

struct dog d;
int main()
{
   d.aa.e = 50;
