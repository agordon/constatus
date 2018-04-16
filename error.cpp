// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

void error_exit(const bool se, const char *format, ...)
{
	int e = errno;
	va_list ap;

	va_start(ap, format);
	char *temp = NULL;
	vasprintf(&temp, format, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", temp);
	syslog(LOG_ERR, "%s", temp);

	if (se && e) {
		fprintf(stderr, "errno: %d (%s)\n", e, strerror(e));
		syslog(LOG_ERR, "errno: %d (%s)", e, strerror(e));
	}

	free(temp);

	exit(EXIT_FAILURE);
}
