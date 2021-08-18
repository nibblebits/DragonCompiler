struct FILE
{

};

typedef struct FILE FILE;

#define __wur
#define __restrict


extern FILE *fopen (const char *__restrict __filename,
		    const char *__restrict __modes) __wur;

int main()
{
    fopen("./test.c", "r");
    return 0;
}