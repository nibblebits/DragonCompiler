struct FILE
{

};

typedef struct FILE FILE;


extern FILE *fopen (const char * __filename,
		    const char * restrict __modes);

int main()
{
    fopen("./test.c", "r");
    return 0;
}