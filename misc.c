#include "misc.h"


int str_matches(const char* input, const char* input2, char delim)
{
    char c = *input;
    char c2 = *input2;
    
    int res = 0;
    while(1)
    {
        // We reached the delimeter or null terminator? Then we go.
        if (c == delim || c == 0x00)
        {
            break;
        }

        if (c != c2)
        {
            res = -1;
            break;
        }

        input++;
        input2++;
    }

    return res;
}