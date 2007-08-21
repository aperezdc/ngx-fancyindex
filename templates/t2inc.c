/*
 * t2h.c
 * Copyright (C) 2007 acastro <acastro@slump>
 *
 * Distributed under terms of the MIT license.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#define MAX_W 70

int
main(int argc, char **argv)
{
	(void) argc; /* unused */
	(void) argv; /* unused */

	int c = 0;
	int w = 0;

	putchar('"');
	while ((c = getchar()) != EOF) {
		/* skip whitespace */
		if ((c != ' ') && isspace(c)) continue;

		/* take care of long lines */
		if (w++ >= MAX_W) {
			printf("\"\n\"");
			w = 0;
		}

		/* escape non-printable characters */
		if (!isprint(c)) {
			printf("\\x%02x", c);
			continue;
		}

#define C(_m, _p) case _m : printf(_p); break

		switch (c) {
			C('\\', "\\\\"); /* backslashes */
			C('"' , "\\\""); /* double quotes */
			default:
				putchar(c);
		}
	}
	printf("\"\n");

	/* check whether an error was produced */
	if ((c == EOF) && ferror(stdin)) {
		fprintf(stderr, "%s: error reading input, %s\n", argv[0], strerror(errno));
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

