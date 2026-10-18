
#include "UserService.h"

#include <iostream>
#include <limits>
#include <string>
#include <memory>

#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

using namespace std;

void addUser(sql::Connection* con)
{
    string name, email, passwordHash, role, phone;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter name: ";
    getline(cin, name);

    cout << "Enter email: ";
    getline(cin, email);

    cout << "Enter password hash: ";
    getline(cin, passwordHash);

    cout << "Enter role (Student/Warden/Maintenance/Admin): ";
    getline(cin, role);

    cout << "Enter phone: ";
    getline(cin, phone);

    if (role != "Student" && role != "Warden" &&
        role != "Maintenance" && role != "Admin")
    {
        cout << "Invalid role!\n";
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "INSERT INTO users "
            "(name, email, password_hash, role, phone) "
            "VALUES (?, ?, ?, ?, ?)"
        )
    );

    stmt->setString(1, name);
    stmt->setString(2, email);
    stmt->setString(3, passwordHash);
    stmt->setString(4, role);
    stmt->setString(5, phone);

    stmt->executeUpdate();

    cout << "User added successfully!\n";
}

void viewUsers(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> result(
        stmt->executeQuery(
            "SELECT user_id, name, email, role, phone "
            "FROM users ORDER BY user_id"
        )
    );

    cout << "\n--- Registered Users ---\n";

    bool found = false;

    while (result->next())
    {
        found = true;

        cout << "\nUser ID: " << result->getInt("user_id");
        cout << "\nName: " << result->getString("name");
        cout << "\nEmail: " << result->getString("email");
        cout << "\nRole: " << result->getString("role");
        cout << "\nPhone: " << result->getString("phone");
        cout << "\n----------------------\n";
    }

    if (!found)
        cout << "No users registered yet.\n";
}

void searchUser(sql::Connection* con)
{
    int id;

    cout << "Enter User ID: ";
    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid User ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "SELECT user_id, name, email, role, phone "
            "FROM users WHERE user_id = ?"
        )
    );

    stmt->setInt(1, id);

    unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    if (result->next())
    {
        cout << "\nUser found!\n";
        cout << "ID: " << result->getInt("user_id") << "\n";
        cout << "Name: " << result->getString("name") << "\n";
        cout << "Email: " << result->getString("email") << "\n";
        cout << "Role: " << result->getString("role") << "\n";
        cout << "Phone: " << result->getString("phone") << "\n";
    }
    else
    {
        cout << "No user found with that ID.\n";
    }
}

void updateUser(sql::Connection* con)
{
    int id;

    cout << "Enter User ID to update: ";
    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid User ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string name, email, role, phone;

    cout << "Enter new name: ";
    getline(cin, name);

    cout << "Enter new email: ";
    getline(cin, email);

    cout << "Enter new role (Student/Warden/Maintenance/Admin): ";
    getline(cin, role);

    cout << "Enter new phone: ";
    getline(cin, phone);

    if (role != "Student" && role != "Warden" &&
        role != "Maintenance" && role != "Admin")
    {
        cout << "Invalid role!\n";
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "UPDATE users SET name = ?, email = ?, "
            "role = ?, phone = ? WHERE user_id = ?"
        )
    );

    stmt->setString(1, name);
    stmt->setString(2, email);
    stmt->setString(3, role);
    stmt->setString(4, phone);
    stmt->setInt(5, id);

    int rows = stmt->executeUpdate();

    if (rows > 0)
        cout << "User updated successfully!\n";
    else
        cout << "User not found or no changes made.\n";
}

void deleteUser(sql::Connection* con)
{
    int id;

    cout << "Enter User ID to delete: ";
    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid User ID.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    char confirm;
    cout << "Are you sure you want to delete this user? (y/n): ";
    cin >> confirm;

    if (confirm != 'y' && confirm != 'Y')
    {
        cout << "Deletion cancelled.\n";
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "DELETE FROM users WHERE user_id = ?"
        )
    );

    stmt->setInt(1, id);

    int rows = stmt->executeUpdate();

    if (rows > 0)
        cout << "User deleted successfully!\n";
    else
        cout << "User not found.\n";
}

void userManagementMenu(sql::Connection* con)
{
    int choice;

    do
    {
        cout << "\n===== USER MANAGEMENT =====\n";
        cout << "1. Add User\n";
        cout << "2. View All Users\n";
        cout << "3. Search User by ID\n";
        cout << "4. Update User\n";
        cout << "5. Delete User\n";
        cout << "6. Back to Main Menu\n";
        cout << "Enter your choice: ";

        if (!(cin >> choice))
        {
            cout << "Please enter a number.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            choice = 0;
            continue;
        }

        try
        {
            switch (choice)
            {
                case 1: addUser(con); break;
                case 2: viewUsers(con); break;
                case 3: searchUser(con); break;
                case 4: updateUser(con); break;
                case 5: deleteUser(con); break;
                case 6: cout << "Returning to main menu...\n"; break;
                default: cout << "Invalid choice.\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cerr << "Database error: " << e.what() << "\n";
        }

    } while (choice != 6);
}
