#include "ComplaintService.h"

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

using namespace std;

// 1. Add a fee record
void addFee(sql::Connection* con)
{
    int userId;
    double amount;
    string dueDate;

    cout << "\nEnter Student User ID: ";
    if (!(cin >> userId) || userId <= 0)
    {
        cout << "Invalid student ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cout << "Enter fee amount: ";
    if (!(cin >> amount) || amount <= 0)
    {
        cout << "Fee amount must be greater than zero.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cout << "Enter due date (YYYY-MM-DD): ";
    cin >> dueDate;

    try
    {
        // Verify that the student exists
        unique_ptr<sql::PreparedStatement> checkStudent(
            con->prepareStatement(
                "SELECT user_id FROM users "
                "WHERE user_id = ? AND role = 'Student'"
            )
        );

        checkStudent->setInt(1, userId);

        unique_ptr<sql::ResultSet> studentResult(
            checkStudent->executeQuery()
        );

        if (!studentResult->next())
        {
            cout << "Student not found. Check the Student User ID.\n";
            return;
        }

        // Insert the fee
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO fees "
                "(user_id, amount, due_date, payment_date, status) "
                "VALUES (?, ?, ?, NULL, 'Pending')"
            )
        );

        stmt->setInt(1, userId);
        stmt->setDouble(2, amount);
        stmt->setString(3, dueDate);

        stmt->executeUpdate();

        cout << "Fee record added successfully!\n";
    }
    catch (sql::SQLException& e)
    {
        cout << "Unable to add fee: " << e.what() << "\n";
    }
}

// 2. View all fee records
void viewFees(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> res(
        stmt->executeQuery(
            "SELECT f.fee_id, f.user_id, u.name, f.amount, "
            "f.due_date, f.payment_date, "
            "CASE "
            "WHEN f.status = 'Paid' THEN 'Paid' "
            "WHEN f.due_date < CURRENT_DATE THEN 'Overdue' "
            "ELSE f.status END AS display_status "
            "FROM fees f "
            "JOIN users u ON f.user_id = u.user_id "
            "ORDER BY f.fee_id"
        )
    );

    cout << "\n========== ALL FEE RECORDS ==========\n";

    bool found = false;

    while (res->next())
    {
        found = true;

        cout << "\nFee ID: " << res->getInt("fee_id")
             << "\nStudent ID: " << res->getInt("user_id")
             << "\nStudent Name: " << res->getString("name")
             << "\nAmount: " << fixed << setprecision(2)
             << res->getDouble("amount")
             << "\nDue Date: " << res->getString("due_date")
             << "\nPayment Date: " << res->getString("payment_date")
             << "\nStatus: " << res->getString("display_status")
             << "\n------------------------------------\n";
    }

    if (!found)
        cout << "No fee records found.\n";
}

// 3. Search fees by student ID
void searchFees(sql::Connection* con)
{
    int userId;

    cout << "Enter Student User ID: ";

    if (!(cin >> userId) || userId <= 0)
    {
        cout << "Invalid student ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "SELECT fee_id, amount, due_date, payment_date, "
            "CASE "
            "WHEN status = 'Paid' THEN 'Paid' "
            "WHEN due_date < CURRENT_DATE THEN 'Overdue' "
            "ELSE status END AS display_status "
            "FROM fees WHERE user_id = ? "
            "ORDER BY fee_id"
        )
    );

    stmt->setInt(1, userId);

    unique_ptr<sql::ResultSet> res(stmt->executeQuery());

    bool found = false;

    while (res->next())
    {
        found = true;

        cout << "\nFee ID: " << res->getInt("fee_id")
             << "\nAmount: " << fixed << setprecision(2)
             << res->getDouble("amount")
             << "\nDue Date: " << res->getString("due_date")
             << "\nPayment Date: " << res->getString("payment_date")
             << "\nStatus: " << res->getString("display_status")
             << "\n";
    }

    if (!found)
        cout << "No fee records found for this student.\n";
}

// 4. Record a fee payment
void recordPayment(sql::Connection* con)
{
    int feeId;

    cout << "Enter Fee ID to record payment: ";

    if (!(cin >> feeId) || feeId <= 0)
    {
        cout << "Invalid fee ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    try
    {
        unique_ptr<sql::PreparedStatement> checkStmt(
            con->prepareStatement(
                "SELECT status FROM fees WHERE fee_id = ?"
            )
        );

        checkStmt->setInt(1, feeId);

        unique_ptr<sql::ResultSet> result(
            checkStmt->executeQuery()
        );

        if (!result->next())
        {
            cout << "Fee record not found.\n";
            return;
        }

        if (result->getString("status") == "Paid")
        {
            cout << "This fee has already been paid.\n";
            return;
        }

        unique_ptr<sql::PreparedStatement> updateStmt(
            con->prepareStatement(
                "UPDATE fees SET payment_date = CURRENT_DATE, "
                "status = 'Paid' "
                "WHERE fee_id = ? AND status <> 'Paid'"
            )
        );

        updateStmt->setInt(1, feeId);
        updateStmt->executeUpdate();

        cout << "Payment recorded successfully!\n";
    }
    catch (sql::SQLException& e)
    {
        cout << "Unable to record payment: " << e.what() << "\n";
    }
}

// 5. View pending and overdue fees
void viewPendingFees(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> res(
        stmt->executeQuery(
            "SELECT f.fee_id, u.name, f.user_id, f.amount, "
            "f.due_date, "
            "CASE "
            "WHEN f.due_date < CURRENT_DATE THEN 'Overdue' "
            "ELSE 'Pending' END AS display_status "
            "FROM fees f "
            "JOIN users u ON f.user_id = u.user_id "
            "WHERE f.status <> 'Paid' "
            "ORDER BY f.due_date"
        )
    );

    cout << "\n======= PENDING / OVERDUE FEES =======\n";

    bool found = false;

    while (res->next())
    {
        found = true;

        cout << "\nFee ID: " << res->getInt("fee_id")
             << "\nStudent: " << res->getString("name")
             << "\nStudent ID: " << res->getInt("user_id")
             << "\nAmount: " << fixed << setprecision(2)
             << res->getDouble("amount")
             << "\nDue Date: " << res->getString("due_date")
             << "\nStatus: " << res->getString("display_status")
             << "\n-------------------------------------\n";
    }

    if (!found)
        cout << "No pending or overdue fees!\n";
}

// Main fee management menu
void feeManagementMenu(sql::Connection* con)
{
    int choice = 0;

    do
    {
        cout << "\n========== FEE MANAGEMENT ==========\n";
        cout << "1. Add Fee Record\n";
        cout << "2. View All Fees\n";
        cout << "3. Search Fees by Student ID\n";
        cout << "4. Record Payment\n";
        cout << "5. View Pending / Overdue Fees\n";
        cout << "6. Back to Main Menu\n";
        cout << "Enter choice: ";

        if (!(cin >> choice))
        {
            cout << "Please enter a number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        try
        {
            switch (choice)
            {
                case 1:
                    addFee(con);
                    break;

                case 2:
                    viewFees(con);
                    break;

                case 3:
                    searchFees(con);
                    break;

                case 4:
                    recordPayment(con);
                    break;

                case 5:
                    viewPendingFees(con);
                    break;

                case 6:
                    break;

                default:
                    cout << "Invalid choice.\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cout << "Database error: " << e.what() << "\n";
        }

    } while (choice != 6);
}
