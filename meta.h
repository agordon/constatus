#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

class meta;
typedef void *(* init_plugin_t)(meta *const m, const char *const argument);
typedef void (* uninit_plugin_t)(void *arg);

typedef struct
{
	init_plugin_t init_plugin;
	uninit_plugin_t uninit_plugin;
	void *handle, *arg;
} meta_plugin_t;

class meta
{
private:
	std::mutex m_int_lock;
	std::map<std::string, std::pair<uint64_t, int> > m_int;
	std::mutex m_double_lock;
	std::map<std::string, std::pair<uint64_t, double> > m_double;
	std::mutex m_string_lock;
	std::map<std::string, std::pair<uint64_t, std::string> > m_string;

	std::vector<meta_plugin_t> plugins;

public:
	meta();
	~meta();

	void add_plugin(const std::string & filename, const std::string & parameter);

	bool get_int(const std::string & key, std::pair<uint64_t, int> *const val);
	void set_int(const std::string & key, const std::pair<uint64_t, int> & v);

	bool get_double(const std::string & key, std::pair<uint64_t, double> *const val);
	void set_double(const std::string & key, const std::pair<uint64_t, double> & v);

	bool get_string(const std::string & key, std::pair<uint64_t, std::string> *const val);
	void set_string(const std::string & key, const std::pair<uint64_t, std::string> & v);
};
