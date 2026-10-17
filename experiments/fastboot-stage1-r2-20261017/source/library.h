/* GPL-2.0-or-later */
#ifndef LIBRARY_H
#define LIBRARY_H
#include <stddef.h>
#define LIBRARY_MAX 2048
#define RECENT_MAX 10
typedef struct { char **paths; size_t count; int truncated; } Library;
int library_scan(Library *lib, const char *directory);
void library_free(Library *lib);
int recent_load(Library *lib, const char *file);
int recent_record(const char *file, const char *path);
const char *library_name(const char *path);
#endif
