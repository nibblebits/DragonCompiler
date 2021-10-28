
struct y
{
    int a;
    int b;
};
struct o
{
    int y;
    int d;
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
int x;
int main()
{
    d.a.oo.yy->b = 50;

}