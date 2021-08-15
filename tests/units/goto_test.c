int main()
{
    int res;
    res = 10;
    goto here;
    res = 12;
here:
    return res;
}