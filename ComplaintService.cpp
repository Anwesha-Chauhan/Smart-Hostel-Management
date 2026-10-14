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

// Register a new complaint
void addComplaint(sql::Connection* con)
{
    int studentId;
    string category, description;

    cout << "\nEnter Student ID: ";
    cin >> studentId;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    try {
        unique_ptr<sql::PreparedStatement> check(
            con->prepareStatement(
                "SELECT user_id FROM users "
                "WHERE user_id = ? AND role = 'Student'"
            )
        );

        check->setInt(1, studentId);
        unique_ptr<sql::ResultSet> res(check->executeQuery());

        if (!res->next()) {
            cout << "Invalid Student ID!\n";
            return;
        }

        cout << "Enter complaint category: ";
        getline(cin, category);

        cout << "Enter complaint description: ";
        getline(cin, description);

        if (category.empty() || description.empty()) {
            cout << "Category and description cannot be empty.\n";
            return;
        }

        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO complaints "
                "(user_id, category, description, status) "
                "VALUES (?, ?, ?, 'Pending')"
            )
        );

        stmt->setInt(1, studentId);
        stmt->setString(2, category);
        stmt->setString(3, description);
        stmt->executeUpdate();

        cout << "Complaint registered successfully!\n";
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Display all complaints
void viewComplaints(sql::Connection* con)
{
    try {
        unique_ptr<sql::Statement> stmt(con->createStatement());

        unique_ptr<sql::ResultSet> res(
            stmt->executeQuery(
                "SELECT c.complaint_id, c.user_id, u.name, "
                "c.category, c.description, c.status, "
                "COALESCE(s.name, 'Not assigned') AS assigned_name, "
                "c.created_at "
                "FROM complaints c "
                "JOIN users u ON c.user_id = u.user_id "
                "LEFT JOIN users s ON c.assigned_to = s.user_id "
                "ORDER BY c.complaint_id"
            )
        );

        cout << "\n========== ALL COMPLAINTS ==========\n";

        while (res->next()) {
            cout << "\nComplaint ID: " << res->getInt("complaint_id")
                 << "\nStudent ID: " << res->getInt("user_id")
                 << "\nStudent Name: " << res->getString("name")
                 << "\nCategory: " << res->getString("category")
                 << "\nDescription: " << res->getString("description")
                 << "\nStatus: " << res->getString("status")
                 << "\nAssigned To: " << res->getString("assigned_name")
                 << "\nCreated At: " << res->getString("created_at")
                 << "\n-----------------------------------\n";
        }
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Search complaints by student ID
void searchComplaints(sql::Connection* con)
{
    int studentId;
    cout << "\nEnter Student ID: ";
    cin >> studentId;

    try {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT complaint_id, category, description, status "
                "FROM complaints WHERE user_id = ? "
                "ORDER BY complaint_id"
            )
        );

        stmt->setInt(1, studentId);
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());

        bool found = false;

        while (res->next()) {
            found = true;

            cout << "\nComplaint ID: " << res->getInt("complaint_id")
                 << "\nCategory: " << res->getString("category")
                 << "\nDescription: " << res->getString("description")
                 << "\nStatus: " << res->getString("status")
                 << "\n";
        }

        if (!found)
            cout << "No complaints found for this student.\n";
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Update complaint status
void updateComplaintStatus(sql::Connection* con)
{
    int complaintId, choice;
    string status;

    cout << "\nEnter Complaint ID: ";
    cin >> complaintId;

    cout << "1. Pending\n";
    cout << "2. In Progress\n";
    cout << "3. Resolved\n";
    cout << "Select new status: ";
    cin >> choice;

    switch (choice) {
        case 1: status = "Pending"; break;
        case 2: status = "In Progress"; break;
        case 3: status = "Resolved"; break;
        default:
            cout << "Invalid choice!\n";
            return;
    }

    try {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "UPDATE complaints SET status = ? "
                "WHERE complaint_id = ?"
            )
        );

        stmt->setString(1, status);
        stmt->setInt(2, complaintId);

        int rows = stmt->executeUpdate();

        if (rows > 0)
            cout << "Complaint status updated successfully!\n";
        else
            cout << "Complaint ID not found, or status is unchanged.\n";
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Assign a complaint to Warden or Maintenance staff
void assignComplaint(sql::Connection* con)
{
    int complaintId, staffId;

    cout << "\nEnter Complaint ID: ";
    cin >> complaintId;

    cout << "Enter Warden/Maintenance User ID: ";
    cin >> staffId;

    try {
        unique_ptr<sql::PreparedStatement> check(
            con->prepareStatement(
                "SELECT user_id FROM users "
                "WHERE user_id = ? "
                "AND role IN ('Warden', 'Maintenance')"
            )
        );

        check->setInt(1, staffId);
        unique_ptr<sql::ResultSet> res(check->executeQuery());

        if (!res->next()) {
            cout << "Invalid staff ID. Enter a Warden or Maintenance ID.\n";
            return;
        }

        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "UPDATE complaints SET assigned_to = ? "
                "WHERE complaint_id = ?"
            )
        );

        stmt->setInt(1, staffId);
        stmt->setInt(2, complaintId);

        int rows = stmt->executeUpdate();

        if (rows > 0)
            cout << "Complaint assigned successfully!\n";
        else
            cout << "Complaint ID not found, or assignment is unchanged.\n";
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Delete a complaint
void deleteComplaint(sql::Connection* con)
{
    int complaintId;

    cout << "\nEnter Complaint ID to delete: ";
    cin >> complaintId;

    try {
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "DELETE FROM complaints WHERE complaint_id = ?"
            )
        );

        stmt->setInt(1, complaintId);

        int rows = stmt->executeUpdate();

        if (rows > 0)
            cout << "Complaint deleted successfully!\n";
        else
            cout << "Complaint ID not found.\n";
    }
    catch (sql::SQLException& e) {
        cout << "Database error: " << e.what() << '\n';
    }
}

// Complaint Management Menu
void complaintManagementMenu(sql::Connection* con)
{
    int choice;

    do {
        cout << "\n====== COMPLAINT MANAGEMENT ======\n";
        cout << "1. Register Complaint\n";
        cout << "2. View All Complaints\n";
        cout << "3. Search by Student ID\n";
        cout << "4. Update Complaint Status\n";
        cout << "5. Assign Complaint to Staff\n";
        cout << "6. Delete Complaint\n";
        cout << "7. Back to Main Menu\n";
        cout << "Enter your choice: ";
        cin >> choice;

        switch (choice) {
            case 1: addComplaint(con); break;
            case 2: viewComplaints(con); break;
            case 3: searchComplaints(con); break;
            case 4: updateComplaintStatus(con); break;
            case 5: assignComplaint(con); break;
            case 6: deleteComplaint(con); break;
            case 7: cout << "Returning to main menu...\n"; break;
            default: cout << "Invalid choice!\n";
        }

    } while (choice != 7);
}
