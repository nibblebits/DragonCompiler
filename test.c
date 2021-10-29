
struct y
{
    int a;
    int b;
};
struct o
{
    int y;
    char d;
    struct y* yy;
    
};
struct s
{
    int e;
    struct o oo;
    char dd;

};
struct dog
{
    int y;
    int kk;
    struct s a;
    int x;

};

struct dog d;
int main()
{
    struct y mm;
    d.a.oo.yy = &mm;
    d.a.oo.yy->b = 50;
    return mm.b;

}