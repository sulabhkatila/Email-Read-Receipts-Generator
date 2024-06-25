#include "DB.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_SQL_QUERY_LEN 512
#define SIGNATURE_RECEIPT_TABLE "signature_receipts"

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

        sprintf(sql, "INSERT INTO %s(ip, from_email, to_email, subject, n_code, date, time) VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s')", SIGNATURE_RECEIPT_TABLE, dbargs->ip, dbargs->from, dbargs->to, dbargs->subject, dbargs->n_code, dbargs->date, dbargs->time);
    
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

