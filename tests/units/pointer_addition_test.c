
int main(int argc, char** argv)
{
    int e;
    e = 20;
    int y;
    y = 50;
    int* x;
    x = &y;

    return *(x+1);
}   
