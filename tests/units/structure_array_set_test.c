struct cat
{
    int x;
    int y;
    int d;
};

struct dog
{
    int x;
    struct cat c;
};


struct dog abc[5];
int main()
{  
    int d;
    abc[3].c.x = 50;
    int c;
    c = 3;
    return abc[c].c.x;
}