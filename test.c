struct y
{
    int dd;
    int ii;
    int ee;
    int bb;
};

struct dog
{
    struct y yy;
};

// bug exists as we dont align structure when calculating stack size...
// now ee is integer lets try again... all should work..
int test( int second, struct y yy)
{
    return yy.bb + yy.ii + second;
}

struct dog dd;
int main()
{
    dd.yy.bb = 10;
    dd.yy.ii = 12;
    return test(10, dd.yy);
}
