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
#include <stdarg.h>

#include "common.h"
#include "messageheader.h"

// extern
int tmpfilescount = 0;

char percentdecode(const char *e)
{
	char tmp[3];
	char *endptr;
	char c;

	if (e[0] == '\0' || e[1] == '\0')
		exitwithstatus(400, "cannot decode percent encoded string");

	tmp[0] = e[0];
	tmp[1] = e[1];
	tmp[2] = '\0';

	c = strtol(tmp, &endptr, 16);

	if (errno == ERANGE || tmp != (endptr - 2))
		exitwithstatus(400, "cannot decode percent encoded string");

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
			exitwithstatus(400, "unexpected symbol in url encoded string");
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

void fprint(FILE *f, const char *s)
{
	size_t len;

	len = strlen(s);

	if (fwrite(s, 1, len, f) < len)
		exitwithstatus(500, "");
}

void fprintnquoted(FILE *f, const char *s, size_t sz)
{
	const char *b, *e;

	b = s;
	while ((e = memchr(b, '\'', b + sz - b)) != NULL) {
		if (fwrite(b, 1, e - b, f) < e - b)
			exitwithstatus(500, "");

		fprintf(f, "'\"'\"'");
		
		b = e + 1;
	}

	if (fwrite(b, 1, s + sz - b, f) < s + sz - b)
		exitwithstatus(500, "");
}

void urlencodedforms(char *forms, FILE *fifofile)
{
	char *attr, *val;

	fprint(fifofile, "typeset -A formdata;\n\n");
	
	while (urldecode(forms, &attr, &val) >= 0) {
		fprint(fifofile, "formdata[");
		fprint(fifofile, attr);
		fprint(fifofile, "]='");
		fprintnquoted(fifofile, val, strlen(val));
		fprint(fifofile, "';\n");	
		
		forms = NULL;
	}
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
			exitwithstatus(400, "cannot read body of multipart data");
	
		if (strcasecmp(field, "Content-Disposition") == 0) {
			char *name, *val;

			hdr->disptype = mh_getdisptype(body);
		
			while ((name = mh_getparam(&val)) != NULL) {
				if (strcasecmp(name, "name") == 0)
					hdr->formname = val;
				else if (strcasecmp(name, "filename") == 0)
					hdr->filename = val;
			}	
		}
		else if (strcasecmp(field, "Content-Type") == 0)
			hdr->conttype = mh_getcontenttype(body);
	}
}

void multipartdata(FILE *fifofile, char *boundary, size_t contlen)
{
	char *s;
	size_t ssz;
	char *bnd;
	size_t bndlen;
	int filen;
	int r;

	fprint(fifofile, "typeset -A formdata;\ntypeset -A filename;\n\n");
	
	bnd = xrealloc(NULL, strlen("--\r\n") + strlen(boundary) + 1);
	sprintf(bnd, "--%s", boundary);
	bndlen = strlen(bnd);
	
	s = NULL;
	do {	
		if ((r = getline(&s, &ssz, stdin)) < 0)
			exitwithstatus(400, "boundary not found while reading multipart data");
	} while (strncmp(s, bnd, bndlen) != 0);
	

	sprintf(bnd, "\r\n--%s", boundary);
	bndlen = strlen(bnd);	

	filen = 0;
	while (1) {
		struct partheader hdr;
		char e[2];

		multipartheader(&hdr);

		if (hdr.formname == NULL || hdr.disptype == NULL)
			exitwithstatus(400, "cannot read header of multipart data block");

		// TODO can i rely on Content-Type field existance
		// when checking source of data?
		if (hdr.conttype == NULL) {
			char cc;
			char *bp;
			
			//TODO can be ' in formname?
			fprint(fifofile, "formdata[");
			fprint(fifofile, hdr.formname);
			fprint(fifofile, "]='");
			
			bp = bnd;
			while (*bp != '\0') {
				int c;
			
				if ((c = getc(stdin)) == EOF)
					exitwithstatus(400, "unexpected EOF while reading multipart data");
				else if (c == *bp)
					++bp;
				else {
					cc = c;
				
					fprintnquoted(fifofile, bnd, bp - bnd);
					
					bp = bnd;
					if (c == *bp) {
						++bp;
						continue;
					}

					fprintnquoted(fifofile, &cc, 1);
				}
			}
			
			fprint(fifofile, "';\n");
		}
		else {
			FILE *tmpfile;
			int tmpfilefd;
			char *bp;
			
			if (tmpfilescount >= MAX_TEMP_FILES)
				exitwithstatus(500, "");

			sprintf(tmpfiles[tmpfilescount], "/tmp/ksh-cgi_%d%d",
				(int) getpid(), filen++);
			
			if ((tmpfilefd = open(tmpfiles[tmpfilescount],
				O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) < 0)
				exitwithstatus(500, "");

			if ((tmpfile = fdopen(tmpfilefd, "w")) == NULL)
				exitwithstatus(500, "");

			bp = bnd;
			while (*bp != '\0') {
				int c;
			
				if ((c = getc(stdin)) == EOF)
					exitwithstatus(400, "unexpected EOF while reading multipart data");
				else if (c == *bp)
					++bp;
				else {
					fwrite(bnd, bp - bnd, 1, tmpfile);
				
					bp = bnd;
					if (c == *bp) {
						++bp;
						continue;
					}	
						
					putc(c, tmpfile);
				}
			}
			
			fclose(tmpfile);

			fprint(fifofile, "formdata[");
			fprint(fifofile, hdr.formname);
			fprint(fifofile, "]='");
			fprintnquoted(fifofile, tmpfiles[tmpfilescount],
				strlen(tmpfiles[tmpfilescount]));
			fprint(fifofile, "';\n");
	
			if (hdr.filename != NULL) {
				fprint(fifofile, "filename[");
				fprint(fifofile, hdr.formname);
				fprint(fifofile, "]='");
				fprintnquoted(fifofile, hdr.filename,
					strlen(hdr.filename));
				fprint(fifofile, "';\n");
			}
			
			++tmpfilescount;
		}

		if (hdr.disptype != NULL)
			free(hdr.disptype);
		if (hdr.conttype != NULL)
			free(hdr.conttype);

		e[0]=getc(stdin);
		while ((e[1] = getc(stdin)) != EOF
			&& strncmp(e, "\r\n", 2) != 0
			&& strncmp(e, "--", 2) != 0)
			e[0] = e[1];
	
		if (e[1] == EOF)
			exitwithstatus(400, "unexpected EOF while reading multipart data");

		if (strncmp(e, "--", 2) == 0)
			break;
	}

	free(bnd);
}
void postmethod(FILE *fifofile)
{
	char *type;
	char *endptr;
	size_t contlen;
		
	if (getenv("CONTENT_TYPE") == NULL)
		exitwithstatus(400, "CONTENT_TYPE field is empty in post request");

	contlen = strtol(getenv("CONTENT_LENGTH"), &endptr, 10);
	if (errno == ERANGE || getenv("CONTENT_LEGTH") == endptr)
		exitwithstatus(400, "CONTENT_LENGTH field is out of range in post request");

	type = mh_getcontenttype(getenv("CONTENT_TYPE"));
	if (strcasecmp(type, "application/x-www-form-urlencoded") == 0) {
		char *body, *p;
		int r;

		if ((body = xrealloc(NULL, contlen + 1)) == NULL)
			exitwithstatus(500, "");

		p = body;
		while ((r = read(0, p, contlen)) != 0) {
			if (r < 0)
				exitwithstatus(500, "");
			
			p += r;
		}
	
		*p = '\0';

		urlencodedforms(body, fifofile);
		
		free(body);
	} else if (strcasecmp(type, "multipart/form-data") == 0) {
		char *name, *val;
		char *boundary;
		
		while ((name = mh_getparam(&val)) != NULL)
			if (strcasecmp(name, "boundary") == 0)
				boundary = val;
	
		multipartdata(fifofile, boundary, contlen);
	}
	else
		exitwithstatus(501, "");

	free(type);
}

int outputproc()
{
	char *s;
	size_t ssz;
	char buf[4096];
	int r;

	ssz = 0;	
	if (getline(&s, &ssz, stdin) < 0)
		exitwithstatus(500, "");

	if (strcmp(s, "<!DOCTYPE html>") == 0)
		if (printf("%s\r\n\r\n", "Content-Type: text/html; charset=utf-8") < 0)
			exitwithstatus(500, "");
		
	if (printf("%s", s) < 0)
		exitwithstatus(500, "");
	
	// transmit other ksh output data
	while ((r = fread(buf, 1, sizeof(buf), stdin)) != 0) {
		if (r < 0)
			return 1;

		fwrite(buf, 1, r, stdout);
	}
	
	return 0;
}

int main(int argc, char *argv[], char *envp[])
{
	FILE *fifofile;
	FILE *scriptfile;
	int cpid;
	int oprocpid;
	char buf[4096];
	int p[2];
	int status;
	int r;
	
	// making fifo for translating script
	// with initilized form values to ksh
	if (sprintf(fifopath, "/tmp/ksh-cgi_%d.fifo", (int) getpid()) < 0)
		exitwithstatus(500, "");

	if (mkfifo(fifopath, S_IRWXU) < 0)
		exitwithstatus(500, "cannot create fifo");

	// ksh output is transmited through pipe to separate
	// process that parsing output.
	if (pipe(p) < 0)
		exitwithstatus(500, "cannot open pipe");

	// run ksh
	if ((cpid = fork()) == 0) {
		close(STDOUT_FILENO);
		dup(p[1]);
	
		close(p[0]);
		close(p[1]);

		argv[0] = "/bin/ksh";
		argv[1] = fifopath;
		execve("/bin/ksh", argv, envp);

		return 1;
	}
	
	// run output parsing process
	if ((oprocpid = fork()) == 0) {
		close(STDIN_FILENO);
		dup(p[0]);
		
		close(p[0]);
		close(p[1]);

		return outputproc();
	}

	close(p[0]);
	close(p[1]);

	// write code to initilize form values before main script
	// for file upload form also create temporary file
	if ((fifofile = fopen(fifopath, "w")) < 0)
		exitwithstatus(500, "cannot create fifo");

	if (strcmp(getenv("REQUEST_METHOD"), "GET") == 0) {
		// RFC 3875 says that this enviroment variable MUST be set,
		// lighttpd doesn't care. Maybe there are exits another RFC.
	
		if (getenv("QUERY_STRING") != NULL)
			urlencodedforms(getenv("QUERY_STRING"), fifofile);
	}
	else if (strcmp(getenv("REQUEST_METHOD"), "POST") == 0)
		postmethod(fifofile);
	
	// write main script
	if ((scriptfile = fopen(argv[1], "r")) < 0)
		exitwithstatus(500, "cannot open script file");
 
	while ((r = fread(buf, 1, sizeof(buf), scriptfile)) != 0) {
		if (r < 0)
			exitwithstatus(500, "error while reading script file");

		fwrite(buf, 1, r, fifofile);
	}
	
	fclose(fifofile);
	
	// wait for ksh process and output processing to end
	wait(&status);
	
	if (status != 0)
		exitwithstatus(500, "script ended with error");

	wait(&status);

	if (status != 0)
		exitwithstatus(500, "script ended with error");

	cleanup();

	
	return 0;
}
