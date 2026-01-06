#pragma once
#include <string>
#include "writer.hpp"

// Forward declare for other files.
// sqlite3 definition is only needed inside db.cpp.
struct sqlite3;

// DB lifecycle
bool db_open(sqlite3** out_db, const std::string& db_path);
void db_close(sqlite3* db);

// Queue API (writer -> DB)
void push_row(const Row& row);

// DB worker thread
void db_thread(sqlite3* db);
