char* abc[4];
int main()
{
    abc[0] = "hello";
    abc[1] = "world";

    char c;
    c = abc[1][1];
    return c; 
}