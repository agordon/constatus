#include <sqlite3.h>
#include <string>
#include <vector>

#include "error.h"
#include "config.h"
#include "db.h"

#ifdef WITH_SQLITE3
int sl_callback(void *cb_data, int argc, char **argv, char **column_names)
{
	std::vector<std::string> row;

	for(int index=0; index<argc; index++)
	{
		if (argv[index])
			row.push_back(argv[index]);
		else
			row.push_back("");
	}

	auto result_set = (std::vector<std::vector<std::string> > *)cb_data;

	result_set -> push_back(row);

	return 0;
}

int get_query_value(sqlite3 *const db, const std::string & query)
{
	std::vector<std::vector<std::string> > results;
	char *error = NULL;

	if (sqlite3_exec(db, query.c_str(), sl_callback, (void *)&results, &error))
		error_exit(false, "DB error: %s", error);

	return atoi(results.at(0).at(0).c_str());
}

std::vector<std::vector<std::string> > * do_query_value(sqlite3 *const db, const std::string & query)
{
	auto results = new std::vector<std::vector<std::string> >;
	char *error = NULL;

	if (sqlite3_exec(db, query.c_str(), sl_callback, results, &error))
		error_exit(false, "DB error: %s", error);

	return results;
}

void do_query(sqlite3 *const db, const std::string & query)
{
	std::vector<std::vector<std::string> > results;
	char *error = NULL;

	if (sqlite3_exec(db, query.c_str(), sl_callback, &results, &error))
		error_exit(false, "DB error: %s", error);
}

bool check_table_exists(sqlite3 *db, std::string table)
{
	if (get_query_value(db, "SELECT COUNT(*) AS n FROM sqlite_master WHERE type='table' AND name='" + table + "'") == 1)
		return true;

	return false;
}

void start_transaction(sqlite3 *db)
{
	char *error = NULL;
	if (sqlite3_exec(db, "begin", NULL, NULL, &error))
		error_exit(false, "DB error: %s", error);
}

void commit_transaction(sqlite3 *db)
{
	char *error = NULL;
	if (sqlite3_exec(db, "commit", NULL, NULL, &error))
		error_exit("DB error: %s", error);
}
#endif

db::db(const std::string & file)
{
#ifdef WITH_SQLITE3
	dbh = NULL;

	if (file.empty())
		return;

	if (sqlite3_open(file.c_str(), &dbh) != SQLITE_OK)
		error_exit(false, "Cannot open/create database file %s", file.c_str());

	if (!check_table_exists(dbh, "events"))
		do_query(dbh, "CREATE TABLE events(id text not null, ts_start datetime not null, ts_end datetime not null, type integer not null, parameter1 text)");

	std::string ins_query = "INSERT INTO events(id, ts_start, ts_end, type, parameter1) VALUES(?, datetime(?, 'unixepoch'), datetime(?, 'unixepoch'), ?, ?)";
	if (sqlite3_prepare_v2(dbh, ins_query.c_str(), -1, &stmt_insert, NULL))
		error_exit(false, "DB error generating prepared statement");
#endif
}

db::~db()
{
#ifdef WITH_SQLITE3
	if (dbh)
		sqlite3_close(dbh);
#endif
}

void db::register_event(const std::string & id, const int type, const std::string & parameter, const struct timeval *const ts, const struct timeval *const te)
{
#ifdef WITH_SQLITE3
	if (!dbh)
		return;

	if (sqlite3_reset(stmt_insert))
		error_exit(false, "sqlite3_reset on stmt_insert failed");

	int err = 0;
	if ((err = sqlite3_bind_text(stmt_insert, 1, id.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_insert, 2, ts -> tv_sec)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_insert, 3, te -> tv_sec)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_insert, 4, type)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_text(stmt_insert, 5, parameter.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_insert failed: %s", sqlite3_errmsg(dbh));

	if (sqlite3_step(stmt_insert) != SQLITE_DONE)
		error_exit(false, "sqlite3_step failed");
#endif
}
