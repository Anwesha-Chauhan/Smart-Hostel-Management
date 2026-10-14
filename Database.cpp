
#include "Database.h"
#include <jdbc/mysql_driver.h>

std::unique_ptr<sql::Connection> connectDatabase()
{
    sql::mysql::MySQL_Driver* driver =
        sql::mysql::get_mysql_driver_instance();

    std::unique_ptr<sql::Connection> con(
        driver->connect(
            "tcp://127.0.0.1:3306",
            "root",
            "root"
        )
    );

    con->setSchema("smart_hostel_db");

    return con;
}
