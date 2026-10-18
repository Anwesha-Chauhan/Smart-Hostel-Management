#include "AttendenceService.h"

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <limits>

#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>


using namespace std;

void attendenceManagementMenu(sql::Connection* con)
{
    int choice;

    do
    {
        cout << "\n===== ATTENDANCE MANAGEMENT =====\n";
        cout << "1. Mark Attendance\n";
        cout << "2. View All Attendance\n";
        cout << "3. Search Attendance by User ID\n";
        cout << "4. Delete Attendance Record\n";
        cout << "5. Back to Main Menu\n";
        cout << "Enter your choice: ";

        if (!(cin >> choice))
        {
            cout << "Please enter a valid number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        try
        {
            if (choice == 1)
            {
                int userId;
                string date, status;

                cout << "Enter User ID: ";
                cin >> userId;

                cout << "Enter date (YYYY-MM-DD): ";
                cin >> date;

                cout << "Enter status (Present/Absent): ";
                cin >> status;

                if (status != "Present" && status != "Absent")
                {
                    cout << "Invalid status. Use Present or Absent.\n";
                    continue;
                }

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "INSERT INTO attendance (user_id, `date`, status) "
                        "VALUES (?, ?, ?)"
                    )
                );

                pstmt->setInt(1, userId);
                pstmt->setString(2, date);
                pstmt->setString(3, status);
                pstmt->executeUpdate();

                cout << "Attendance marked successfully!\n";
            }
            else if (choice == 2)
            {
                unique_ptr<sql::Statement> stmt(con->createStatement());

                unique_ptr<sql::ResultSet> res(
                    stmt->executeQuery(
                        "SELECT attendance_id, user_id, `date`, status, recorded_at "
                        "FROM attendance ORDER BY `date`, user_id"
                    )
                );

                cout << "\nID\tUser ID\tDate\t\tStatus\t\tRecorded At\n";
                cout << "-------------------------------------------------------------\n";

                while (res->next())
                {
                    cout << res->getInt("attendance_id") << "\t"
                         << res->getInt("user_id") << "\t"
                         << res->getString("date") << "\t"
                         << res->getString("status") << "\t\t"
                         << res->getString("recorded_at") << "\n";
                }
            }
            else if (choice == 3)
            {
                int userId;

                cout << "Enter User ID: ";
                cin >> userId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "SELECT attendance_id, user_id, `date`, status, recorded_at "
                        "FROM attendance WHERE user_id = ? ORDER BY `date`"
                    )
                );

                pstmt->setInt(1, userId);

                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

                cout << "\nID\tUser ID\tDate\t\tStatus\n";
                cout << "----------------------------------------\n";

                bool found = false;

                while (res->next())
                {
                    found = true;

                    cout << res->getInt("attendance_id") << "\t"
                         << res->getInt("user_id") << "\t"
                         << res->getString("date") << "\t"
                         << res->getString("status") << "\n";
                }

                if (!found)
                    cout << "No attendance records found for this user.\n";
            }
            else if (choice == 4)
            {
                int attendanceId;

                cout << "Enter Attendance ID to delete: ";
                cin >> attendanceId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "DELETE FROM attendance WHERE attendance_id = ?"
                    )
                );

                pstmt->setInt(1, attendanceId);

                int rows = pstmt->executeUpdate();

                if (rows > 0)
                    cout << "Attendance record deleted successfully.\n";
                else
                    cout << "Attendance record not found.\n";
            }
            else if (choice == 5)
            {
                cout << "Returning to main menu...\n";
            }
            else
            {
                cout << "Invalid choice. Try again.\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cerr << "Database error: " << e.what() << "\n";
        }

    } while (choice != 5);
}
