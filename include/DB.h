#ifndef _DB_H_
#define _DB_H_

#include <sqlite3.h>

// table member will specify the table in the database
// log_to_db will access other members based on the table specified
typedef struct db_args {
    char *db_path;
    char *table;
    char *ip;
    char *from;
    char *to;
    char *subject;
    char *n_code;
    char *date;
    char *time;
} db_args;

// To allow for multithreaded approach
void *log_to_db(void *args);    // *args need to be able to be type casted to *db_args 

#endif

