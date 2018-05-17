all:
	gcc ksh-cgi.c common.c messageheader.c -Wall -o ksh-cgi
install:
	cp ksh-cgi /usr/bin/ksh-cgi
