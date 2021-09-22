
struct dog
{
   struct nn
   {
      int e;
      int dd;
      struct ff
      {
         int x;
      } ff;
   } aa;
   int k;
};

int main()
{
  struct dog d;
  d.aa.dd = 10;
  d.aa.e = 20;
  d.aa.ff.x = 90;
  d.k = 40;

}