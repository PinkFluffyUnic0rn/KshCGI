#ifndef MESSAGE_HEADER_H
#define MESSAGE_HEADER_H

enum MH_ERROR_TYPE {
	MH_SUCCESS,
	MH_EOF,
	MH_ENDQUOTE,
	MH_UNQUOTEDSPEC,
	MH_WRONGTOKEN
};

extern int mh_errcode;

char *mh_getfield();

char *mh_fieldbody(char *field);

char *mh_getcontenttype(const char *s);

char *mh_getdisptype(const char *s);

char *mh_getparam(char **val);

#endif
