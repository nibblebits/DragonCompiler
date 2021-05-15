
struct bob
{
   int i;
   int p;
   int z;
   int o;
};

struct animal
{
   int e;
};
struct dog
{
   int o;
   int z;
   int d;
   struct animal a;
};

struct hello
{
   int a;
   struct dog c;
   int b;
};

int main()
{
   struct hello h;

   h.c.a.e = 50;
}
