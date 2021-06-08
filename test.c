struct elf
{
    char arr[50];
};

struct dog
{
    char k;
    struct elf e;
    char m;


};
int e = 10;
int main()
{
   struct dog d;
   e = d.e.arr[20];


}