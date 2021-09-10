#define ABC 26
#define CBA ABC

#if CBA == 26
int main()
{
    return 26;
}
#else
int main()
{
    return 20;
}
#endif