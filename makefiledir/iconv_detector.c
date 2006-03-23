/* $Id$ */

#include <stdlib.h>
#include <iconv.h>
#include <stdio.h>

/* this is a pretty simple app, that will return 1 if it manages to compile and execute
 * This means that it can be used by the makefile to detect if iconv is present on the current system
 * no iconv means this file fails and will return nothing */

int main ()
{
	iconv_t cd = iconv_open("","");
	iconv(cd,NULL,NULL,NULL,NULL);
	iconv_close(cd);
	printf("1\n");
	return 0;
}
