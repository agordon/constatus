#include <sqlite3.h>
#include <string>
#include "config.h"

typedef enum { EVT_MOTION } event_t;

class db
{
private:
#ifdef WITH_SQLITE3
	sqlite3 *dbh;
	sqlite3_stmt *stmt_event_insert, *stmt_event_update, *stmt_file_insert;
#endif

public:
	db(const std::string & file);
	~db();

	unsigned long register_event(const std::string & id, const int type, const std::string & parameter, const struct timeval *const ts);
	void update_event(const unsigned long event_nr, const struct timeval *const te);
	void register_file(const std::string & id, const unsigned long event_nr, const std::string & file);
};
