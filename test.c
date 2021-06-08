
struct dog
{
    int a;
    int bb;
    char k;
    int ii;
};
struct animal
{
    int a;
    struct dog d;
};


int e = 10;
int main()
{
    struct animal dog;
    
    e = dog.a;
    e = dog.d.a;
     e = dog.d.bb;
    e = dog.d.k;
    e = dog.d.ii;

}