
int printf(const char* s, ...);
int main()
{
    float x;
    double y;
    y = 50.99;
    long long xx;
    xx = y;
    y = (double) xx;
    printf("%f", y);
}
