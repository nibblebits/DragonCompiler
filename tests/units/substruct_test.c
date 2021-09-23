

struct dog
{

   struct nn
   {
      int e;
      int dd;
   } aa;
};
int main()
{
   struct dog d;
   d.aa.e = 10;
   return d.aa.e;
}