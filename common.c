#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "common.h"

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
