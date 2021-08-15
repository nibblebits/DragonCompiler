int main()
{
    int res;
    res = 10;
    if (1)
    {
        int dd;
        dd = 10;
        goto here;
    }
    res = 12;
here:
    return res;
}