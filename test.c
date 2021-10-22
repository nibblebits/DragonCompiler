int abc[8];
int main()
{
    int c;
    int d;
    d = 1;
    c = 5;
    abc[1] = 20;
    abc[c] = abc[d];
    return abc[c];
}