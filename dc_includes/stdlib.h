#ifndef STDLIB_H
#define STDLIB_H
#define NULL 0
typedef long size_t;
typedef long wchar_t;
typedef long ptrdiff_t;

int atoi(const char *str);
long int atol(const char *str);
long int strtol(const char *str, char **endptr, int base);
unsigned long int strtoul(const char *str, char **endptr, int base);
void *calloc(size_t nitems, size_t size);
void free(void *ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void abort(void);
void exit(int status);
char *getenv(const char *name);
int system(const char *string);
int abs(int x);
long int labs(long int x);
int rand(void);
void srand(unsigned int seed);
int mblen(const char *str, size_t n);

#endif