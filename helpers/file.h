#ifndef FILE_H
#define FILE_H

struct dc_file* dc_fopen(const char* filename);
void dc_fclose(struct dc_file* file);
struct vector* dc_read_until(struct dc_file* file, const char* delims);
char dc_read(struct dc_file* file);

#endif