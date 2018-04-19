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
	stmt_event_insert = stmt_event_update = stmt_file_insert = NULL;

	if (file.empty())
		return;

	if (sqlite3_open(file.c_str(), &dbh) != SQLITE_OK)
		error_exit(false, "Cannot open/create database file %s", file.c_str());

	/// events
	if (!check_table_exists(dbh, "events"))
		do_query(dbh, "CREATE TABLE events(nr INTEGER PRIMARY KEY AUTOINCREMENT, id text not null, ts_start datetime not null, ts_end datetime, type integer not null, parameter1 text)");

	std::string ins_event_query = "INSERT INTO events(id, ts_start, type, parameter1) VALUES(?, datetime(?, 'unixepoch'), ?, ?)";
	if (sqlite3_prepare_v2(dbh, ins_event_query.c_str(), -1, &stmt_event_insert, NULL))
		error_exit(false, "DB error generating prepared statement");

	/// events update
	std::string upd_event_query = "UPDATE events SET ts_end=datetime(?, 'unixepoch') WHERE nr=?";
	if (sqlite3_prepare_v2(dbh, upd_event_query.c_str(), -1, &stmt_event_update, NULL))
		error_exit(false, "DB error generating prepared statement");

	/// event-files
	if (!check_table_exists(dbh, "event_files"))
		do_query(dbh, "CREATE TABLE event_files(event_nr int not null, id text not null, file text not null)");

	std::string ins_file_query = "INSERT INTO event_files(event_nr, id, file) VALUES(?, ?, ?)";
	if (sqlite3_prepare_v2(dbh, ins_file_query.c_str(), -1, &stmt_file_insert, NULL))
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

unsigned long db::register_event(const std::string & id, const int type, const std::string & parameter, const struct timeval *const ts)
{
#ifdef WITH_SQLITE3
	if (!dbh)
		return 0;

	if (sqlite3_reset(stmt_event_insert))
		error_exit(false, "sqlite3_reset on stmt_event_insert failed");

	int err = 0;
	if ((err = sqlite3_bind_text(stmt_event_insert, 1, id.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_event_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_event_insert, 2, ts -> tv_sec)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_event_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_event_insert, 3, type)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_event_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_text(stmt_event_insert, 4, parameter.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_event_insert failed: %s", sqlite3_errmsg(dbh));

	if (sqlite3_step(stmt_event_insert) != SQLITE_DONE)
		error_exit(false, "sqlite3_step(event insert) failed: %s", sqlite3_errmsg(dbh));

	return sqlite3_last_insert_rowid(dbh);
#else
	return 0;
#endif
}

void db::update_event(const unsigned long event_nr, const struct timeval *const te)
{
#ifdef WITH_SQLITE3
	if (!dbh)
		return;

	if (sqlite3_reset(stmt_event_update))
		error_exit(false, "sqlite3_reset on stmt_event_update failed");

	int err = 0;
	if ((err = sqlite3_bind_int(stmt_event_update, 1, te -> tv_sec)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_event_update failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_int(stmt_event_update, 2, event_nr)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_event_update failed: %s", sqlite3_errmsg(dbh));

	if (sqlite3_step(stmt_event_update) != SQLITE_DONE)
		error_exit(false, "sqlite3_step(event update) failed: ", sqlite3_errmsg(dbh));
#endif
}

void db::register_file(const std::string & id, const unsigned long event_nr, const std::string & file)
{
#ifdef WITH_SQLITE3
	if (!dbh)
		return;

	if (sqlite3_reset(stmt_file_insert))
		error_exit(false, "sqlite3_reset on stmt_file_insert failed");

	int err = 0;
	if ((err = sqlite3_bind_int(stmt_file_insert, 1, event_nr)) != SQLITE_OK)
		error_exit("sqlite3_bind_int stmt_file_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_text(stmt_file_insert, 2, id.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_file_insert failed: %s", sqlite3_errmsg(dbh));

	if ((err = sqlite3_bind_text(stmt_file_insert, 3, file.c_str(), -1, SQLITE_STATIC)) != SQLITE_OK)
		error_exit("sqlite3_bind_text stmt_file_insert failed: %s", sqlite3_errmsg(dbh));

	if (sqlite3_step(stmt_file_insert) != SQLITE_DONE)
		error_exit(false, "sqlite3_step(file insert) failed: %s", sqlite3_errmsg(dbh));
#endif
}
