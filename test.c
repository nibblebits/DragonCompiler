// Lets implement ability to return strucutres...

// but first ability to set structures to structs


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
    return abc[3].c.x;

}