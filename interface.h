#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "meta.h"

// C++ has no instanceOf
typedef enum { CT_HTTPSERVER, CT_MOTIONTRIGGER, CT_TARGET, CT_SOURCE, CT_LOOPBACK, CT_NONE } classtype_t;

class interface
{
protected:
	std::timed_mutex pause_lock;
	std::atomic_bool paused;
	std::thread *th;
	std::atomic_bool local_stop_flag;

	classtype_t ct;
	std::string id, d;

	void pauseCheck();

public:
	interface(const std::string & id);
	virtual ~interface();

	static meta * get_meta();

	std::string gen_id() const;
	std::string get_id_str() const;
	std::string get_description() const { return d; }
	classtype_t get_class_type() const { return ct; }

	void start();
	bool is_running() const { return th != NULL; }
	bool pause();
	bool is_paused() const { return paused; }
	void unpause();
	void stop();

	virtual void operator()() = 0;
};
