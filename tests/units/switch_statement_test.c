

int main()
{

    int res;
    res = 0;
    if (1)
    {
        int m;
        m = 1;
        switch (m)
        {

        case 1:
            switch(5)
            {
                case 1:
                    res = 5;
                break;
                case 5:
                    res = 10;
                break;
            }
            break;

        case 4:
            res = 4;
            break;
        case 2:
            res = 2;
            break;
        }
    }

    return res;
}