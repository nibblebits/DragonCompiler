
struct dog
{
   int x;
};


struct c
{
   int x;
   int y;
};

struct cat
{
   struct dog d;
   struct c b;
};


int main()
{
   struct cat c;
   return 0;
}