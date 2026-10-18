#include "VisitorService.h"

#include <iostream>
#include <string>
#include <memory>
#include <limits>

#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>


using namespace std;

void visitorManagementMenu(sql::Connection* con)
{
    int choice = 0;

    do
    {
        cout << "\n===== VISITOR MANAGEMENT =====\n";
        cout << "1. Register Visitor\n";
        cout << "2. View All Visitors\n";
        cout << "3. Search Visitors by Student ID\n";
        cout << "4. Check Out Visitor\n";
        cout << "5. Delete Visitor Record\n";
        cout << "6. Back to Main Menu\n";
        cout << "Enter your choice: ";

        if (!(cin >> choice))
        {
            cout << "Please enter a valid number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            choice = 0;
            continue;
        }

        try
        {
            // 1. Register visitor
            if (choice == 1)
            {
                int studentId;
                string visitorName, relation;

                cout << "Enter Student ID: ";
                cin >> studentId;

                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Enter Visitor Name: ";
                getline(cin, visitorName);

                cout << "Enter Relation to Student: ";
                getline(cin, relation);

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "INSERT INTO visitors "
                        "(student_id, visitor_name, relation) "
                        "VALUES (?, ?, ?)"
                    )
                );

                pstmt->setInt(1, studentId);
                pstmt->setString(2, visitorName);
                pstmt->setString(3, relation);

                pstmt->executeUpdate();

                cout << "Visitor registered successfully!\n";
            }

            // 2. View all visitors
            else if (choice == 2)
            {
                unique_ptr<sql::Statement> stmt(
                    con->createStatement()
                );

                unique_ptr<sql::ResultSet> res(
                    stmt->executeQuery(
                        "SELECT visitor_id, student_id, visitor_name, "
                        "relation, check_in, check_out "
                        "FROM visitors ORDER BY visitor_id"
                    )
                );

                cout << "\nVisitor ID\tStudent ID\tVisitor Name\t"
                     << "Relation\tCheck-in\t\tCheck-out\n";

                cout << "----------------------------------------------------------------"
                     << "--------------------\n";

                bool found = false;

                while (res->next())
                {
                    found = true;

                    cout << res->getInt("visitor_id") << "\t\t"
                         << res->getInt("student_id") << "\t\t"
                         << res->getString("visitor_name") << "\t"
                         << res->getString("relation") << "\t"
                         << res->getString("check_in") << "\t"
                         << res->getString("check_out") << "\n";
                }

                if (!found)
                    cout << "No visitor records found.\n";
            }

            // 3. Search visitors by student ID
            else if (choice == 3)
            {
                int studentId;

                cout << "Enter Student ID: ";
                cin >> studentId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "SELECT visitor_id, student_id, visitor_name, "
                        "relation, check_in, check_out "
                        "FROM visitors WHERE student_id = ? "
                        "ORDER BY visitor_id"
                    )
                );

                pstmt->setInt(1, studentId);

                unique_ptr<sql::ResultSet> res(
                    pstmt->executeQuery()
                );

                bool found = false;

                cout << "\nVisitor ID\tVisitor Name\tRelation\t"
                     << "Check-in\t\tCheck-out\n";

                while (res->next())
                {
                    found = true;

                    cout << res->getInt("visitor_id") << "\t\t"
                         << res->getString("visitor_name") << "\t"
                         << res->getString("relation") << "\t"
                         << res->getString("check_in") << "\t"
                         << res->getString("check_out") << "\n";
                }

                if (!found)
                    cout << "No visitors found for this student.\n";
            }

            // 4. Check out visitor
            else if (choice == 4)
            {
                int visitorId;

                cout << "Enter Visitor ID to check out: ";
                cin >> visitorId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "UPDATE visitors SET check_out = CURRENT_TIMESTAMP "
                        "WHERE visitor_id = ? AND check_out IS NULL"
                    )
                );

                pstmt->setInt(1, visitorId);

                int rows = pstmt->executeUpdate();

                if (rows > 0)
                    cout << "Visitor checked out successfully!\n";
                else
                    cout << "Visitor not found or already checked out.\n";
            }

            // 5. Delete visitor record
            else if (choice == 5)
            {
                int visitorId;

                cout << "Enter Visitor ID to delete: ";
                cin >> visitorId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "DELETE FROM visitors WHERE visitor_id = ?"
                    )
                );

                pstmt->setInt(1, visitorId);

                int rows = pstmt->executeUpdate();

                if (rows > 0)
                    cout << "Visitor record deleted successfully.\n";
                else
                    cout << "Visitor record not found.\n";
            }

            else if (choice == 6)
            {
                cout << "Returning to main menu...\n";
            }

            else
            {
                cout << "Invalid choice. Please try again.\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cerr << "Database error: " << e.what() << "\n";
        }
    }
    while (choice != 6);
}
