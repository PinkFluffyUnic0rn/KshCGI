#ifndef DSTRING_H
#define DSTRING_H

#include <stddef.h>

struct dstring {
	char *str;
	size_t sz;
	size_t maxsz;
};

void dstringinit(struct dstring *s);

void dstringcat(struct dstring *s, const char *c, ...);

void dstringqncat(struct dstring *s, char *c, size_t sz);

void dstringqcat(struct dstring *s, char *c);

void dstringdestroy(struct dstring *s);

#endif
