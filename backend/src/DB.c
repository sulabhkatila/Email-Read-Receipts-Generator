#include "DB.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SQL_QUERY_LEN 512
#define SIGNATURE_RECEIPT_TABLE "signature_receipts"

// Database functions //

// Log to database
void *log_to_db(void *args) {
    db_args *dbargs = (db_args *)args;
    sqlite3 *db;
    char *err_msg = 0;

    if (strcmp(dbargs->table, SIGNATURE_RECEIPT_TABLE) == 0) {
        int status = sqlite3_open(dbargs->db_path, &db);
        if (status != SQLITE_OK) {
            fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(1);
        }

        char sql[MAX_SQL_QUERY_LEN];

        sprintf(
            sql,
            "INSERT INTO %s(ip, from_email, to_email, subject, n_code, date, "
            "time) VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s')",
            SIGNATURE_RECEIPT_TABLE, dbargs->ip, dbargs->from, dbargs->to,
            dbargs->subject, dbargs->n_code, dbargs->date, dbargs->time);

        status = sqlite3_exec(db, sql, 0, 0, &err_msg);
        if (status != SQLITE_OK) {
            fprintf(stderr, "Failed to insert record: %s\n", err_msg);
            sqlite3_free(err_msg);
        }

    } else {
        fprintf(stderr, "Table does not exist: %s\n", dbargs->table);
    }

    sqlite3_close(db);
    return NULL;
}

// Check if an entry exists
int number_of_entries(db_args *dbargs) {
    sqlite3 *db;
    int count = 0;

    if (strcmp(dbargs->table, SIGNATURE_RECEIPT_TABLE) == 0) {
        int status = sqlite3_open(dbargs->db_path, &db);
        if (status != SQLITE_OK) {
            fprintf(stderr, "Cannot open databse: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(1);
        }

        char sql_query[MAX_SQL_QUERY_LEN];
        sprintf(
            sql_query,
            "SELECT COUNT(*) FROM %s WHERE from_email = '%s' AND "
            "to_email = '%s' AND subject = '%s' AND n_code = '%s';",
            dbargs->table, dbargs->from, dbargs->to,
            dbargs->subject, dbargs->n_code);

        sqlite3_stmt *res;

        status = sqlite3_prepare_v2(db, sql_query, -1, &res, 0);
        if (status != SQLITE_OK) {
            fprintf(stderr, "Failed to execute statement: %s\n",
                    sqlite3_errmsg(db));
            sqlite3_close(db);
            exit(1);
        }

        status = sqlite3_step(res);
        if (status == SQLITE_ROW) {
            count = sqlite3_column_int(res, 0);
        }

        sqlite3_finalize(res);
        sqlite3_close(db);
    }

    return count;
}
