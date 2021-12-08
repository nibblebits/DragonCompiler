#ifndef STDIO_H
#define STDIO_H
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>

typedef size_t fpos_t;

typedef struct _iobuf
{
    char *_ptr;
    int _cnt;
    char *_base;
    int _flag;
    int _file;
    int _charbuf;
    int _bufsiz;
    char *_tmpfname;
} FILE;

int printf(const char *s, ...);
FILE *fopen(const char *filename, const char *mode);
void clearerr(FILE *stream);
int feof(FILE *stream);
int fclose(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
int fgetpos(FILE *stream, size_t *pos);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fseek(FILE *stream, long int offset, int whence);
int fsetpos(FILE *stream, const fpos_t *pos);
long int ftell(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int remove(const char *filename);
int rename(const char *old_filename, const char *new_filename);
void rewind(FILE *stream);
void setbuf(FILE *stream, char *buffer);
int setvbuf(FILE *stream, char *buffer, int mode, size_t size);
FILE *tmpfile(void);
char *tmpnam(char *str);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list arg);
int vprintf(const char *format, va_list arg);
int vsprintf(char *str, const char *format, va_list arg);
int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int sscanf(const char *str, const char *format, ...);
int fgetc(FILE *stream);
char *fgets(char *str, int n, FILE *stream);
int fputc(char, FILE *stream);
int fputs(const char *str, FILE *stream);
int getc(FILE *stream);
int getchar(void);
char *gets(char *str);
int putc(char, FILE *stream);
int putchar(char);
int puts(const char *str);
int ungetc(char, FILE *stream);
void perror(const char *str);
#endif