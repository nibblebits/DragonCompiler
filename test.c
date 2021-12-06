
int main(int argc, char** argv)
{
    char e;
    e = 20;
    char y;
    y = 50;
    char* x;
    x = &y;

    return *(x+1);
}   
