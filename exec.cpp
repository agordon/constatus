// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include "error.h"

void exec(const std::string & what, const std::string & parameter)
{
	unshare(CLONE_FILES);

	if (!what.empty()) {
		pid_t p = fork();

		if (p == 0) {
			system((what + " " + parameter).c_str());

			exit(0);
		}
		else if (p == -1) {
			error_exit(true, "Cannot fork");
		}
	}
}

FILE * exec(const std::string & command_line)
{
	FILE *fh = popen(command_line.c_str(), "w");

	if (!fh)
		error_exit(true, "Cannot exec %s", command_line.c_str());

	return fh;
}
