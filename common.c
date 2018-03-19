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

void *xrealloc(void *ptr, size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL)
		exitwithstatus(500);

	return p;
}
