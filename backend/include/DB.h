#ifndef _DB_H_
#define _DB_H_

#include <sqlite3.h>

// table member will specify the table in the database
// log_to_db will access other members based on the table specified
typedef struct db_args {
    const char *db_path;
    const char *table;
    const char *ip;
    const char *from;
    const char *to;
    const char *subject;
    const char *n_code;
    const char *date;
    const char *time;
} db_args;

// To allow for multithreaded approach
void *log_to_db(void *args);    // *args need to be able to be type casted to *db_args 

#endif

