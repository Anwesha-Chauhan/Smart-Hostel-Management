
#ifndef DATABASE_H
#define DATABASE_H

#include <memory>
#include <jdbc/mysql_connection.h>

std::unique_ptr<sql::Connection> connectDatabase();

#endif
