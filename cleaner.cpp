// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <errno.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "cleaner.h"
#include "utils.h"
#include "error.h"
#include "log.h"

cleaner::cleaner(db *const dbi, const int check_interval, const int purge_interval) : dbi(dbi), check_interval(check_interval), purge_interval(purge_interval)
{
	th = NULL;
	local_stop_flag = false;
}

cleaner::~cleaner()
{
}

void cleaner::start()
{
	if (th)
		error_exit(false, "thread already running");

	local_stop_flag = false;

	if (check_interval > 0 && purge_interval > 0)
		th = new std::thread(std::ref(*this));
}

void cleaner::stop()
{
	if (th) {
		local_stop_flag = true;

		th -> join();
		delete th;

		th = NULL;
	}
}

void cleaner::operator()()
{
	log(LL_INFO, "Cleaner thread started");

	for(;!local_stop_flag;) {
		log(LL_INFO, "cleaning (%d interval)", check_interval);
		std::vector<std::string> files = dbi -> purge(purge_interval);

		for(std::string file : files) {
			if (unlink(file.c_str()) == -1)
				log(LL_ERR, "Cannot delete %s: %s", strerror(errno));
		}

		mysleep(check_interval, &local_stop_flag, NULL);
	}

	log(LL_INFO, "Cleaner thread terminating");
}
