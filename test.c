struct y
{
    int dd;
    char ii;
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
    return yy.ii;
}

struct dog dd;
int main()
{
    dd.yy.dd = 0xffffffff;
    dd.yy.ii = 12;
    dd.yy.ee = 0x44444444;
    dd.yy.bb = 0x22222222;

    // Likely an issue will occur as we fail to recognize its a char.
    return test(10, dd.yy);
}
