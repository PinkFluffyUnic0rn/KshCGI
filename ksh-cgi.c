#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/limits.h>

#include "messageheader.h"

// TODO make a config file
char fifopath[PATH_MAX];
#define MAX_TEMP_FILES 32
char tmpfiles[MAX_TEMP_FILES][PATH_MAX];

int tmpfilescount = 0;

void cleanup()
{
	int i;

	unlink(fifopath);

	for (i = 0; i < tmpfilescount; ++i)
		unlink(tmpfiles[i]);
}

// TODO explain errors in status field
void exitwithstatus(int status)
{
	cleanup();
	
	printf("Status: %d\r\n\r\n", status);
	fflush(stdout);

	exit(1);
}

void *xrealloc(void *ptr, size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL)
		exitwithstatus(500);

	return p;
}

// TODO maybe should use buffered I/O from stdlib
void writedata(int fd, char *data, size_t len)
{
	ssize_t w, ww;

	w = 0;
	while ((ww = write(fd, data + w, len - w)) != 0) {
		if (ww < 0)
			exitwithstatus(500);
		
		w += ww;
	}
}

char percentdecode(const char *e)
{
	char tmp[3];
	char *endptr;
	char c;

	if (e[0] == '\0' || e[1] == '\0')
		exitwithstatus(400);

	tmp[0] = e[0];
	tmp[1] = e[1];
	tmp[2] = '\0';

	c = strtol(tmp, &endptr, 16);

	if (errno == ERANGE || tmp != (endptr - 2))
		exitwithstatus(400);

	return c;
}

int isallowed(char a)
{
	if ((a >= 'a' && a <= 'z')
		|| (a >= 'A' && a <= 'Z')
		|| (a >= '0' && a <= '9')
		|| a == '-' || a == '_'
		|| a == '.' || a == '~'
		|| a == '*')
		return 1;

	return 0;
}

void valdecode(char *s)
{
	const char *p;

	for (p = s; *p != '\0'; ++p) {
		if (isallowed(*p))
			*(s++) = *p;
		else if (*p == '+')
			*(s++) = ' ';
		else if (*p == '%') {
			*(s++) = percentdecode(p + 1);

			p += 2;
		}
		else
			exitwithstatus(400);
	}

	*(s++) = '\0';
}

int urldecode(char *s, char **attr, char **value)
{
	static char *t = NULL;

	if (s != NULL)
		t = s;

	if (t == NULL || *t == '\0')
		return (-1);
	
	s = t;
	while (*t != '&' && *t != '\0')
		++t;
	if (*t == '&')
		*(t++) = '\0';

	*attr = s;
	s = strchr(s, '=');
	*(s++) = '\0';
	*value = s;


	valdecode(*attr);
	valdecode(*value);
	
	return 0;
}

struct dstring {
	char *str;
	size_t sz;
	size_t maxsz;
};

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

void dstringcat(struct dstring *s, const char *c)
{
	dstringncat(s, c, strlen(c));
}

void dstringqncat(struct dstring *s, char *c, size_t sz)
{
	char *b, *e;

	b = c;
	while ((e = memchr(b, '\'', b + sz - b)) != NULL) {
		dstringncat(s, b, e - b);
		dstringcat(s, "'\"'\"'");
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

void urlencodedforms(char *forms, int fifofd)
{
	struct dstring tmpstr;
	char *attr, *val;
	
	dstringinit(&tmpstr);
	
	dstringcat(&tmpstr, "typeset -A formdata;\n\n");
	while (urldecode(forms, &attr, &val) >= 0) {
		dstringcat(&tmpstr, "formdata[");
		dstringcat(&tmpstr, attr);
		dstringcat(&tmpstr, "]='");
		dstringqcat(&tmpstr, val);
		dstringcat(&tmpstr, "';\n");
		
		forms = NULL;
	}

	writedata(fifofd, tmpstr.str, strlen(tmpstr.str));
}

struct partheader {
	char *disptype;
	char *formname;
	char *filename;
	char *conttype;
};

void multipartheader(struct partheader *hdr)
{
	char *field;

	hdr->disptype = hdr->formname = hdr->filename = hdr->conttype = NULL;
	while ((field = mh_getfield()) != NULL) {
		char *body;

		if ((body = mh_fieldbody(field)) == NULL)
			exitwithstatus(400);
	
		if (strcasecmp(field, "Content-Disposition") == 0) {
			char *name, *val;

			hdr->disptype = mh_getdisptype(body);
		
			while ((name = mh_getparam(&val)) != NULL) {
				// TODO are parameter names case sensetive?
				if (strcmp(name, "name") == 0)
					hdr->formname = val;
				else if (strcmp(name, "filename") == 0)
					hdr->filename = val;
			}	
		}
		else if (strcasecmp(field, "Content-Type") == 0)
			hdr->conttype = mh_getcontenttype(body);
	}
}

void multipartdata(int fifofd, char *boundary, size_t contlen)
{
	char *s;
	size_t ssz;
	struct dstring tmpstr;
	char *bnd;
	size_t bndlen;
	int filen;
	int r;

	dstringinit(&tmpstr);
	dstringcat(&tmpstr, "typeset -A formdata;\n");
	dstringcat(&tmpstr, "typeset -A filename;\n\n");
	
	bnd = malloc(strlen("--\r\n") + strlen(boundary) + 1);
	sprintf(bnd, "--%s", boundary);
	bndlen = strlen(bnd);
	
	s = NULL;
	do {	
		if ((r = getline(&s, &ssz, stdin)) < 0)
			exitwithstatus(400);
	} while (strncmp(s, bnd, bndlen) != 0);
	

	sprintf(bnd, "\r\n--%s", boundary);
	bndlen = strlen(bnd);	

	filen = 0;
	while (1) {
		struct partheader hdr;
		char e[2];

		multipartheader(&hdr);

		if (hdr.formname == NULL || hdr.disptype == NULL)
			exitwithstatus(400);

		// TODO can i rely on Content-Type field existance
		// when checking source of data?
		if (hdr.conttype == NULL) {
			char cc;
			char *bp;
			
			//TODO can be ' in formname?
			dstringcat(&tmpstr, "formdata[");
			dstringcat(&tmpstr, hdr.formname);
			dstringcat(&tmpstr, "]='");
			
			bp = bnd;
			while (*bp != '\0') {
				int c;
			
				if ((c = getc(stdin)) == EOF)
					exitwithstatus(400);
				else if (c == *bp)
					++bp;
				else {
					cc = c;
				
					dstringqncat(&tmpstr, bnd, bp - bnd);
					dstringqncat(&tmpstr, &cc, 1);
					
					bp = bnd;
				}
			}

			dstringcat(&tmpstr, "';\n");
		}
		else {
			FILE *tmpfile;
			int tmpfilefd;
			char *bp;
				
			sprintf(tmpfiles[tmpfilescount], "/tmp/ksh-cgi_%d%d",
				(int) getpid(), filen++);
			
			if ((tmpfilefd = open(tmpfiles[tmpfilescount],
				O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) < 0)
				exitwithstatus(500);

			if ((tmpfile = fdopen(tmpfilefd, "w")) == NULL)
				exitwithstatus(500);

			bp = bnd;
			while (*bp != '\0') {
				int c;
			
				if ((c = getc(stdin)) == EOF)
					exitwithstatus(400);
				else if (c == *bp)
					++bp;
				else {
					fwrite(bnd, bp - bnd, 1, tmpfile);
					bp = bnd;
					putc(c, tmpfile);
				}
			}

			fclose(tmpfile);


			dstringcat(&tmpstr, "formdata[");
			dstringcat(&tmpstr, hdr.formname);
			dstringcat(&tmpstr, "]='");
			dstringqcat(&tmpstr, tmpfiles[tmpfilescount]);
			dstringcat(&tmpstr, "';\n");
	
			if (hdr.filename != NULL) {	
				dstringcat(&tmpstr, "filename[");
				dstringcat(&tmpstr, hdr.formname);
				dstringcat(&tmpstr, "]='");
				dstringqcat(&tmpstr, hdr.filename);
				dstringcat(&tmpstr, "';\n");
			}
			
			++tmpfilescount;
		}

		e[0]=getc(stdin);
		while ((e[1] = getc(stdin)) > 0
			&& strncmp(e, "\r\n", 2) != 0
			&& strncmp(e, "--", 2) != 0)
			e[0] = e[1];
	
		if (e[1] < 0)
			exitwithstatus(400);

		if (strncmp(e, "--", 2) == 0)
			break;
	}

	
	writedata(fifofd, tmpstr.str, strlen(tmpstr.str));
	dstringdestroy(&tmpstr);


	// TODO free header data

	free(bnd);
}
void postmethod(int fifofd)
{
	char *type;
	char *endptr;
	size_t contlen;
		
	if (getenv("CONTENT_TYPE") == NULL)
		exitwithstatus(400);

	contlen = strtol(getenv("CONTENT_LENGTH"), &endptr, 10);
	if (errno == ERANGE || getenv("CONTENT_LEGTH") == endptr)
		exitwithstatus(400);

	type = mh_getcontenttype(getenv("CONTENT_TYPE"));
	if (strcasecmp(type, "application/x-www-form-urlencoded") == 0) {
		char *body, *p;
		int r;

		if ((body = malloc(contlen)) == NULL)
			exitwithstatus(500);

		p = body;
		while ((r = read(0, p, contlen)) != 0) {
			if (r < 0)
				exitwithstatus(500);
		
			p += r;
		}

		urlencodedforms(body, fifofd);
		
		free(body);
	} else if (strcasecmp(type, "multipart/form-data") == 0) {
		char *name, *val;
		char *boundary;

		while ((name = mh_getparam(&val)) != NULL)
			if (strcasecmp(name, "boundary") == 0)
				boundary = val;
	
		multipartdata(fifofd, boundary, contlen);
	}
	else
		exitwithstatus(501);

	free(type);
}

int main(int argc, char *argv[], char *envp[])
{
	int fifofd;
	int scriptfd;
	int cpid;
	char buf[4096];
	int status;
	int r;

	writedata(1, "Content-Type: text/html; charset=utf-8\r\n\r\n",
		strlen("Content-Type: text/html; charset=utf-8\r\n\r\n"));

	if (sprintf(fifopath, "/tmp/ksh-cgi_%d.fifo", (int) getpid()) < 0)
		exitwithstatus(500);

	if (mkfifo(fifopath, S_IRWXU) < 0)
		exitwithstatus(500);
	
	if ((cpid = fork()) == 0) {
		argv[0] = "/bin/ksh";
		argv[1] = fifopath;
		execve("/bin/ksh", argv, envp);
		
		exitwithstatus(500);
	}

	//TODO try to use buffered I/O fith FIFO	
	if ((fifofd = open(fifopath, O_WRONLY)) < 0)
		exitwithstatus(500);
 
	if (strcmp(getenv("REQUEST_METHOD"), "GET") == 0) {
		// RFC 3875 says that this enviroment variable MUST be set,
		// lighttpd doesn't care. Maybe there are exits another RFC.
	
		if (getenv("QUERY_STRING") != NULL)
			urlencodedforms(getenv("QUERY_STRING"), fifofd);
	}
	else if (strcmp(getenv("REQUEST_METHOD"), "POST") == 0)
		postmethod(fifofd);

	//TODO use fdopen for buffered I/O
	if ((scriptfd = open(argv[1], O_RDONLY)) < 0)
		exitwithstatus(500);
 
	while ((r = read(scriptfd, buf, sizeof(buf))) != 0) {
		if (r < 0)
			exitwithstatus(500);

		writedata(fifofd, buf, r);
	}

	close(fifofd);

	wait(&status);
	
	if (status != 0)
		exitwithstatus(500);

	cleanup();

	
	return 0;
}
