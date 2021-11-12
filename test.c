struct wolf
{
    int e;
    int d;
};
struct dog
{
    int x;
    int y;
    struct wolf d;
};

struct dog d;
struct dog* _ptr;
struct dog** sptr;
int main()
{
    d.y = 50;
    _ptr = &d;
    sptr = &_ptr;
    return (*sptr)->y;
}