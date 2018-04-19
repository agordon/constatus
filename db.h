#include <sqlite3.h>
#include <string>

typedef enum { EVT_MOTION } event_t;

class db
{
private:
#ifdef WITH_SQLITE3
	sqlite3 *dbh;
	sqlite3_stmt *stmt_insert;
#endif

public:
	db(const std::string & file);
	~db();

	void register_event(const std::string & id, const int type, const std::string & parameter, const struct timeval *const ts, const struct   timeval *const te);
};
