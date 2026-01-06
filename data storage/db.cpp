#include "db.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <atomic>

// Toggle SQLite (compile with -DUSE_SQLITE=1)
#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
  #include "third_party/sqlite/sqlite3.h"
#else
  // If you truly want "no stub", always compile with USE_SQLITE=1.
  // This stub exists only so code compiles if someone forgets the flag.
  struct sqlite3 {};
  #define SQLITE_OK 0
#endif

// stop flag from initialisation.cpp
extern std::atomic<bool> g_stop;

// -----------------------
// Queue: writer -> db
// -----------------------
static std::mutex db_mtx;
static std::condition_variable db_cv;
static std::queue<Row> db_q;

void push_row(const Row& row) {
    {
        std::lock_guard<std::mutex> lk(db_mtx);
        db_q.push(row);
    }
    db_cv.notify_one();
}

#if USE_SQLITE

static bool exec_sql(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[DB] SQLite error: " << (err ? err : "(unknown)") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool db_open(sqlite3** out_db, const std::string& db_path) {
    *out_db = nullptr;

    if (sqlite3_open(db_path.c_str(), out_db) != SQLITE_OK) {
        std::cerr << "[DB] Failed to open DB: " << db_path << "\n";
        return false;
    }

    sqlite3_busy_timeout(*out_db, 2000);

    // WAL + balanced safety/perf
    exec_sql(*out_db, "PRAGMA journal_mode=WAL;");
    exec_sql(*out_db, "PRAGMA synchronous=NORMAL;");

    const char* schema =
        "CREATE TABLE IF NOT EXISTS frames ("
        "  frame_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  ts_ns INTEGER NOT NULL UNIQUE,"
        "  width INTEGER NOT NULL,"
        "  height INTEGER NOT NULL,"
        "  bpp INTEGER NOT NULL,"
        "  raw_bytes INTEGER NOT NULL,"
        "  compressed_bytes INTEGER NOT NULL,"
        "  filepath TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_frames_ts ON frames(ts_ns);";

    return exec_sql(*out_db, schema);
}

void db_close(sqlite3* db) {
    if (db) sqlite3_close(db);
}

void db_thread(sqlite3* db) {
    std::cout << "[DB] Thread started (SQLite).\n";

    const char* insert_sql =
        "INSERT OR REPLACE INTO frames "
        "(ts_ns, width, height, bpp, raw_bytes, compressed_bytes, filepath) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[DB] Failed to prepare insert statement: "
                  << sqlite3_errmsg(db) << "\n";
        return;
    }

    const int COMMIT_EVERY = 25;
    int pending = 0;

    exec_sql(db, "BEGIN;");

    while (true) {
        Row row;

        // Wait for data OR shutdown
        {
            std::unique_lock<std::mutex> lk(db_mtx);
            db_cv.wait(lk, [] { return g_stop.load() || !db_q.empty(); });

            // If shutting down and nothing left, exit
            if (db_q.empty() && g_stop.load()) break;

            row = db_q.front();
            db_q.pop();
        }

        // Sentinel from writer_thread
        if (row.ts_ns == 0) break;

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(row.ts_ns));
        sqlite3_bind_int(stmt,   2, static_cast<int>(row.width));
        sqlite3_bind_int(stmt,   3, static_cast<int>(row.height));
        sqlite3_bind_int(stmt,   4, static_cast<int>(row.bpp));
        sqlite3_bind_int(stmt,   5, static_cast<int>(row.raw_bytes));
        sqlite3_bind_int(stmt,   6, static_cast<int>(row.compressed_bytes));
        sqlite3_bind_text(stmt,  7, row.filepath.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "[DB] Insert failed rc=" << rc
                      << " err=" << sqlite3_errmsg(db) << "\n";
        } else {
            pending++;
            if (pending >= COMMIT_EVERY) {
                exec_sql(db, "COMMIT;");
                exec_sql(db, "BEGIN;");
                pending = 0;
            }
        }
    }

    exec_sql(db, "COMMIT;");
    sqlite3_finalize(stmt);

    std::cout << "[DB] Thread exiting.\n";
}

#else

// If USE_SQLITE=0, this will only print rows.
// But for your DB task: compile with -DUSE_SQLITE=1.
bool db_open(sqlite3** out_db, const std::string& db_path) {
    (void)db_path;
    *out_db = nullptr;
    std::cout << "[DB] db_open STUB. (Compile with -DUSE_SQLITE=1)\n";
    return true;
}

void db_close(sqlite3* db) { (void)db; }

void db_thread(sqlite3* db) {
    (void)db;
    std::cout << "[DB] Thread started (STUB).\n";
    while (true) {
        Row row;
        {
            std::unique_lock<std::mutex> lk(db_mtx);
            db_cv.wait(lk, [] { return g_stop.load() || !db_q.empty(); });
            if (db_q.empty() && g_stop.load()) break;
            row = db_q.front();
            db_q.pop();
        }
        if (row.ts_ns == 0) break;
        std::cout << "[DB] (stub) row ts=" << row.ts_ns
                  << " file=" << row.filepath << "\n";
    }
    std::cout << "[DB] Thread exiting.\n";
}

#endif
