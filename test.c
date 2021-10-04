#define mkstr(S) #S

int main()
{
    char* abc;
    abc = mkstr(testing abc);
}