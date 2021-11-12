struct wolf
{
    int e;
    int d;
};
struct dog
{
    int x;
    int* y;
    struct wolf d;
};

struct dog d;
struct dog* _ptr;
struct dog** sptr;
int main()
{
    int yy;
    yy = &d.y;
    return yy;
}