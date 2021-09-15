
struct dog;


struct cat
{
   struct dog* d;
};

struct dog
{
   int e;
   int x;
};


int main()
{
   struct dog c;
   c.x = 50;
   return 10;
}
