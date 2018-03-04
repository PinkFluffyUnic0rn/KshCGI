#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "dstring.h"

static void *xrealloc(void *ptr, size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL)
		exitwithstatus(500);

	return p;
}

void dstringinit(struct dstring *s)
{
	s->str = xrealloc(NULL, 1);

	s->str[0] = '\0';
	s->sz = 1;
	s->maxsz = 1;
}

void dstringncat(struct dstring *s, const char *c, size_t l)
{
	if (s->sz + l >= s->maxsz) {
		s->maxsz = (s->sz + l) * 2;
		s->str = xrealloc(s->str, s->maxsz);
	}

	strncat(s->str, c, l);
	s->sz += l;
}

void dstringcat(struct dstring *s, const char *c, ...)
{
	va_list valist;
	const char *arg;

	va_start(valist, c);
	
	arg = c;	
	do {
		dstringncat(s, arg, strlen(arg));
	} while ((arg = va_arg(valist, const char *)) != NULL);

	va_end(valist);
}

void dstringqncat(struct dstring *s, char *c, size_t sz)
{
	char *b, *e;

	b = c;
	while ((e = memchr(b, '\'', b + sz - b)) != NULL) {
		dstringncat(s, b, e - b);
		dstringcat(s, "'\"'\"'", NULL);
		b = e + 1;
	}

	dstringncat(s, b, c + sz - b);
}

void dstringqcat(struct dstring *s, char *c)
{
	dstringqncat(s, c, strlen(c));
}

void dstringdestroy(struct dstring *s)
{
	free(s->str);
	s->str = NULL;
}
