#include "LeaveService.h"

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

void applyLeave(sql::Connection* con)
{
    int studentId;
    string fromDate, toDate, reason;

    cout << "Enter Student ID: ";
    cin >> studentId;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter From Date (YYYY-MM-DD): ";
    getline(cin, fromDate);

    cout << "Enter To Date (YYYY-MM-DD): ";
    getline(cin, toDate);

    cout << "Enter Reason for Leave: ";
    getline(cin, reason);

    try
    {
        unique_ptr<sql::PreparedStatement> check(
            con->prepareStatement(
                "SELECT user_id FROM users "
                "WHERE user_id = ? AND role = 'Student'"
            )
        );

        check->setInt(1, studentId);
        unique_ptr<sql::ResultSet> result(check->executeQuery());

        if (!result->next())
        {
            cout << "Invalid Student ID!\n";
            return;
        }

        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO leave_requests "
                "(user_id, from_date, to_date, reason, status) "
                "VALUES (?, ?, ?, ?, 'Pending')"
            )
        );

        stmt->setInt(1, studentId);
        stmt->setString(2, fromDate);
        stmt->setString(3, toDate);
        stmt->setString(4, reason);

        stmt->executeUpdate();

        cout << "Leave request submitted successfully!\n";
    }
    catch (sql::SQLException& e)
    {
        cout << "Database Error: " << e.what() << '\n';
    }
}

void viewLeaveRequests(sql::Connection* con)
{
    try
    {
        unique_ptr<sql::Statement> stmt(con->createStatement());

        unique_ptr<sql::ResultSet> result(
            stmt->executeQuery(
                "SELECT l.leave_id, u.name, l.user_id, "
                "l.from_date, l.to_date, l.reason, l.status "
                "FROM leave_requests l "
                "JOIN users u ON l.user_id = u.user_id "
                "ORDER BY l.leave_id"
            )
        );

        cout << "\n--- LEAVE REQUESTS ---\n";

        while (result->next())
        {
            cout << "\nLeave ID: " << result->getInt("leave_id")
                 << "\nStudent: " << result->getString("name")
                 << "\nStudent ID: " << result->getInt("user_id")
                 << "\nFrom: " << result->getString("from_date")
                 << "\nTo: " << result->getString("to_date")
                 << "\nReason: " << result->getString("reason")
                 << "\nStatus: " << result->getString("status")
                 << "\n-------------------------\n";
        }
    }
    catch (sql::SQLException& e)
    {
        cout << "Database Error: " << e.what() << '\n';
    }
}

void searchLeaveByStudent(sql::Connection* con)
{
    int studentId;

    cout << "Enter Student ID: ";
    cin >> studentId;

    try
    {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT leave_id, from_date, to_date, reason, status "
                "FROM leave_requests WHERE user_id = ?"
            )
        );

        stmt->setInt(1, studentId);

        unique_ptr<sql::ResultSet> result(stmt->executeQuery());

        while (result->next())
        {
            cout << "\nLeave ID: " << result->getInt("leave_id")
                 << "\nFrom: " << result->getString("from_date")
                 << "\nTo: " << result->getString("to_date")
                 << "\nReason: " << result->getString("reason")
                 << "\nStatus: " << result->getString("status")
                 << "\n-------------------------\n";
        }
    }
    catch (sql::SQLException& e)
    {
        cout << "Database Error: " << e.what() << '\n';
    }
}

void updateLeaveStatus(sql::Connection* con)
{
    int leaveId, choice;
    string status;

    cout << "Enter Leave ID: ";
    cin >> leaveId;

    cout << "1. Approve\n2. Reject\n";
    cout << "Enter choice: ";
    cin >> choice;

    if (choice == 1)
        status = "Approved";
    else if (choice == 2)
        status = "Rejected";
    else
    {
        cout << "Invalid choice!\n";
        return;
    }

    try
    {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "UPDATE leave_requests SET status = ? "
                "WHERE leave_id = ?"
            )
        );

        stmt->setString(1, status);
        stmt->setInt(2, leaveId);

        int rows = stmt->executeUpdate();

        if (rows > 0)
            cout << "Leave status updated successfully!\n";
        else
            cout << "Leave request not found!\n";
    }
    catch (sql::SQLException& e)
    {
        cout << "Database Error: " << e.what() << '\n';
    }
}

void deleteLeaveRequest(sql::Connection* con)
{
    int leaveId;

    cout << "Enter Leave ID to delete: ";
    cin >> leaveId;

    try
    {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "DELETE FROM leave_requests WHERE leave_id = ?"
            )
        );

        stmt->setInt(1, leaveId);

        int rows = stmt->executeUpdate();

        if (rows > 0)
            cout << "Leave request deleted successfully!\n";
        else
            cout << "Leave request not found!\n";
    }
    catch (sql::SQLException& e)
    {
        cout << "Database Error: " << e.what() << '\n';
    }
}

void leaveManagementMenu(sql::Connection* con)
{
    int choice;

    do
    {
        cout << "\n===== LEAVE MANAGEMENT =====\n";
        cout << "1. Apply for Leave\n";
        cout << "2. View All Leave Requests\n";
        cout << "3. Search by Student ID\n";
        cout << "4. Approve/Reject Leave\n";
        cout << "5. Delete Leave Request\n";
        cout << "6. Back to Main Menu\n";
        cout << "Enter choice: ";
        cin >> choice;

        switch (choice)
        {
            case 1: applyLeave(con); break;
            case 2: viewLeaveRequests(con); break;
            case 3: searchLeaveByStudent(con); break;
            case 4: updateLeaveStatus(con); break;
            case 5: deleteLeaveRequest(con); break;
            case 6: break;
            default: cout << "Invalid choice!\n";
        }
    } while (choice != 6);
}