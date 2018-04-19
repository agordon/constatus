#pragma once

#include <atomic>
#include <thread>

#include "db.h"

class cleaner
{
private:
	std::thread *th;
	std::atomic_bool local_stop_flag;
	db *const dbi;
	const int check_interval, purge_interval;

public:
	cleaner(db *const dbi, const int check_interval, const int purge_interval);
	virtual ~cleaner();

	void start();
	void stop();

	void operator()();
};
