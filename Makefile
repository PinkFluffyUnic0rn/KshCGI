all:
	gcc ksh-cgi.c common.c dstring.c messageheader.c -Wall -o ksh-cgi
