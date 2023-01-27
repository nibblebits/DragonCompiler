struct dog
{
    int x;
};

struct wolf
{
    char d;
};

struct dog* d;
struct dog dd;
int main()
{
    dd.x = 50;
    d = &dd;
    // lol it works :) 
    return ((struct dog*)d)->x;
}

