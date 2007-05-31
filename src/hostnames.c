/*
 * hostnames.c
 * by Keith Gaughan (http://talideon.com/)
 *
 * Lists all the names associated with a given host (or the current one if
 * non is specified.
 *
 * Copyleft (k) Keith Gaughan, 2007.
 * All rights reversed. Do what you will with it, just don't come crying to
 * me if something bad happens.
 *
 * To compile, type: gcc -lc -Os -Wall -o hostnames hostnames.c
 * You might need an implementation of strlcpy().
 */

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

/* Sigh... */
#ifndef FALSE
#define FALSE 0
#define TRUE (!(FALSE))
#endif

static int quiet = FALSE;

static void echo(char* key, char* val) {
	if (quiet) {
		printf("%s\n", val);
	} else {
		printf("%s: %s\n", key, val);
	}
}

int main(int argc, char** argv) {
	char buf[250];
	struct hostent* h;
	char** aliases;
	int arg = 1;

	/* Check for quiet flag */
	if (argc > 1 && strcmp(argv[arg], "-q") == 0) {
		argc--;
		arg++;
		quiet = TRUE;
	}

	if (argc > 1) {
		/* Use the one passed in on the command line. */
		strlcpy(buf, argv[arg], sizeof buf);
	} else if (gethostname(buf, sizeof buf) != 0) {
		perror("Could not read host name");
		return 1;
	}

	h = gethostbyname(buf);
	if (h == NULL) {
		herror("Could not fetch host details");
		return 2;
	}
	echo("Official hostname", h->h_name);
	aliases = h->h_aliases;
	while (*aliases) {
		echo("Alias", *aliases);
		aliases++;
	}

	return 0;
}
