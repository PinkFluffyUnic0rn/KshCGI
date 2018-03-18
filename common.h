#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <linux/limits.h>

// TODO make a config file
char fifopath[PATH_MAX];
#define MAX_TEMP_FILES 32
char tmpfiles[MAX_TEMP_FILES][PATH_MAX];

extern int tmpfilescount;

void cleanup();

void exitwithstatus(int status);

void writedata(int fd, char *data, size_t len);

void *xrealloc(void *ptr, size_t size);

#endif
