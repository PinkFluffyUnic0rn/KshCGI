#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "messageheader.h"

int mh_errcode = MH_SUCCESS;

static int iswhitespace(char a)
{
	// LWSP-char according to RFC 822
	return (a == ' ' || a == '\t');
}

static int iscontrol(char a)
{
	// according to RFC 822
	return (a >= 0 && a <= 31);
}

static int isspecial(char a)
{
	// according to RFC 2045
	return (strchr("()<>@,;:\\\"/[]?=", a) != NULL);
}

static char *mh_error(enum MH_ERROR_TYPE et)
{
	mh_errcode = et;

	return NULL;
}

static char *mh_gettoken(const char *s)
{
	static char *t = NULL;
	static const char *ss = NULL;
	char *prevt;

	if (s != NULL) {
		ss = s;
		t = malloc(2 * strlen(s) + 1);
	}
	
	prevt = t;

	while (iswhitespace(*ss))
		++ss;

	if (*ss == '\0') 
		return mh_error(MH_EOF);
	else if (*ss == '"') {
		++ss;
		while (*ss != '\"') {
			if (*ss == '\0')
				return mh_error(MH_ENDQUOTE);

			// TODO maybe i should detect unquoted specials as error
			// TODO does \0 can be quoted character?
			if ((*ss == '\\') && *(++ss) == '\0')
				return mh_error(MH_ENDQUOTE);

			*(t++) = *(ss++);
		}

		++ss;
	}
	else if (isspecial(*ss)) {
		*(t++) = *(ss++);
	}
	else {
		while(!isspecial(*ss) && !iswhitespace(*ss))
			*(t++) = *(ss++);
	}
	
	*(t++) = '\0';

	return prevt;
}

static char *mh_gettokenkind(const char *s, char c)
{
	char *t;
	
	t = mh_gettoken(s);
	
	if (t == NULL)
		return NULL;

	if (c == '\0' && isspecial(*t))
		return mh_error(MH_WRONGTOKEN);

	if (c != '\0' && *t != c)
		return mh_error(MH_WRONGTOKEN);

	return t;
}

char *mh_getfield()
{
	char *fld;
	size_t fldsz;
	int fldlen;
	char *s;
	size_t ssz;
	char c;

	fld = NULL;
	if ((fldlen = getline(&fld, &fldsz, stdin)) < 0
		|| strcmp("\r\n", fld) == 0) {
		free(fld);
		return NULL;
	}

	s = NULL;
	ssz = 0;
	while ((c = fgetc(stdin), ungetc(c, stdin)) != EOF
		&& iswhitespace(c)) {
		size_t slen;
	
		slen = getline(&s, &ssz, stdin);
		
		if (strcmp("\r\n", fld + fldlen - 2) == 0)
			fldlen -= 2;

		fld = realloc(fld, fldsz + slen);
		
		strcpy(fld + fldlen, s);
		fldlen += slen;
	}

	if (strcmp("\r\n", fld + fldlen - 2) == 0)
		fld[fldlen - 2] = '\0';

	if (s != NULL)
		free(s);

	return fld;
}

char *mh_fieldbody(char *field)
{
	char *body;

	if ((body = strchr(field, ':')) == NULL)
		return NULL;

	*(body++) = '\0';

	for (; *field != '\0'; ++field)
		if (iscontrol(*field) || *field == ' ' || *field == ':')
			return NULL;

	return body;
}

char *mh_getcontenttype(const char *s)
{
	char *type;
	char *t;

	if ((type = mh_gettokenkind(s, '\0')) == NULL)
		return NULL;

	if ((t = mh_gettokenkind(NULL, '/')) == NULL)
		return NULL;
	
	strcat(type, t);

	if ((t = mh_gettokenkind(NULL, '\0')) == NULL)
		return NULL;

	strcat(type, t);
	
	return type;
}

char *mh_getdisptype(const char *s)
{
	char *type;

	if ((type = mh_gettokenkind(s, '\0')) == NULL)
		return NULL;
	
	return type;
}

char *mh_getparam(char **val)
{
	char *name;

	if ((name = mh_gettokenkind(NULL, ';')) == NULL)
		return NULL;
	
	if ((name = mh_gettokenkind(NULL, '\0')) == NULL)
		return NULL;

	if (mh_gettokenkind(NULL, '=') == NULL)
		return NULL;

	if ((*val = mh_gettokenkind(NULL, '\0')) == NULL)
		return NULL;

	return name;
}
