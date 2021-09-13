
struct dog
{
   int x;
};

struct cat
{
   struct dog d;
};


int main()
{
   struct cat c;
   return 0;
}