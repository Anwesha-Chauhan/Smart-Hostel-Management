
#include "RoomService.h"

#include <iostream>
#include <string>
#include <memory>
#include <limits>

#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

using namespace std;

void addRoom(sql::Connection* con)
{
    string roomNumber, block;
    int capacity;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter room number: ";
    getline(cin, roomNumber);

    cout << "Enter hostel block: ";
    getline(cin, block);

    cout << "Enter room capacity: ";

    if (!(cin >> capacity) || capacity <= 0)
    {
        cout << "Invalid capacity!\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "INSERT INTO rooms "
            "(room_number, hostel_block, capacity, "
            "current_occupancy, status) "
            "VALUES (?, ?, ?, 0, 'Available')"
        )
    );

    stmt->setString(1, roomNumber);
    stmt->setString(2, block);
    stmt->setInt(3, capacity);

    stmt->executeUpdate();

    cout << "Room added successfully!\n";
}

void viewRooms(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> result(
        stmt->executeQuery(
            "SELECT room_id, room_number, hostel_block, "
            "capacity, current_occupancy, status "
            "FROM rooms ORDER BY room_id"
        )
    );

    cout << "\n========== ALL ROOMS ==========\n";

    bool found = false;

    while (result->next())
    {
        found = true;

        cout << "\nRoom ID: "
             << result->getInt("room_id");
        cout << "\nRoom Number: "
             << result->getString("room_number");
        cout << "\nBlock: "
             << result->getString("hostel_block");
        cout << "\nCapacity: "
             << result->getInt("capacity");
        cout << "\nCurrent Occupancy: "
             << result->getInt("current_occupancy");
        cout << "\nStatus: "
             << result->getString("status");
        cout << "\n------------------------------\n";
    }

    if (!found)
        cout << "No rooms found.\n";
}

void searchRoom(sql::Connection* con)
{
    int id;

    cout << "Enter Room ID: ";

    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid Room ID!\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "SELECT room_id, room_number, hostel_block, "
            "capacity, current_occupancy, status "
            "FROM rooms WHERE room_id = ?"
        )
    );

    stmt->setInt(1, id);

    unique_ptr<sql::ResultSet> result(stmt->executeQuery());

    if (result->next())
    {
        cout << "\nRoom found!\n";
        cout << "Room ID: " << result->getInt("room_id") << "\n";
        cout << "Room Number: " << result->getString("room_number") << "\n";
        cout << "Block: " << result->getString("hostel_block") << "\n";
        cout << "Capacity: " << result->getInt("capacity") << "\n";
        cout << "Occupancy: " << result->getInt("current_occupancy") << "\n";
        cout << "Status: " << result->getString("status") << "\n";
    }
    else
    {
        cout << "Room not found.\n";
    }
}

void updateRoom(sql::Connection* con)
{
    int id, capacity;
    string roomNumber, block, status;

    cout << "Enter Room ID to update: ";

    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid Room ID!\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter new room number: ";
    getline(cin, roomNumber);

    cout << "Enter new hostel block: ";
    getline(cin, block);

    cout << "Enter new capacity: ";

    if (!(cin >> capacity) || capacity <= 0)
    {
        cout << "Invalid capacity!\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter status (Available/Full/Maintenance): ";
    getline(cin, status);

    if (status != "Available" &&
        status != "Full" &&
        status != "Maintenance")
    {
        cout << "Invalid status!\n";
        return;
    }

    // Read the existing occupancy before changing capacity.
    unique_ptr<sql::PreparedStatement> check(
        con->prepareStatement(
            "SELECT current_occupancy FROM rooms WHERE room_id = ?"
        )
    );

    check->setInt(1, id);

    unique_ptr<sql::ResultSet> result(check->executeQuery());

    if (!result->next())
    {
        cout << "Room not found.\n";
        return;
    }

    int occupancy = result->getInt("current_occupancy");

    if (capacity < occupancy)
    {
        cout << "Capacity cannot be less than current occupancy!\n";
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "UPDATE rooms SET room_number = ?, hostel_block = ?, "
            "capacity = ?, status = ? WHERE room_id = ?"
        )
    );

    stmt->setString(1, roomNumber);
    stmt->setString(2, block);
    stmt->setInt(3, capacity);
    stmt->setString(4, status);
    stmt->setInt(5, id);

    stmt->executeUpdate();

    cout << "Room updated successfully!\n";
}

void viewAvailableRooms(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> result(
        stmt->executeQuery(
            "SELECT room_id, room_number, hostel_block, "
            "capacity, current_occupancy "
            "FROM rooms "
            "WHERE status = 'Available' "
            "AND current_occupancy < capacity "
            "ORDER BY room_id"
        )
    );

    cout << "\n======= AVAILABLE ROOMS =======\n";

    bool found = false;

    while (result->next())
    {
        found = true;

        cout << "\nRoom ID: " << result->getInt("room_id");
        cout << "\nRoom Number: " << result->getString("room_number");
        cout << "\nBlock: " << result->getString("hostel_block");
        cout << "\nRemaining Beds: "
             << result->getInt("capacity") -
                result->getInt("current_occupancy");
        cout << "\n------------------------------\n";
    }

    if (!found)
        cout << "No available rooms found.\n";
}

void deleteRoom(sql::Connection* con)
{
    int id;

    cout << "Enter Room ID to delete: ";

    if (!(cin >> id) || id <= 0)
    {
        cout << "Invalid Room ID!\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    // Refuse to delete rooms with allocation records.
    unique_ptr<sql::PreparedStatement> check(
        con->prepareStatement(
            "SELECT COUNT(*) AS total "
            "FROM room_allocations WHERE room_id = ?"
        )
    );

    check->setInt(1, id);

    unique_ptr<sql::ResultSet> result(check->executeQuery());

    if (result->next() && result->getInt("total") > 0)
    {
        cout << "Cannot delete this room: allocation records exist.\n";
        cout << "Vacate or resolve its allocations first.\n";
        return;
    }

    char confirm;

    cout << "Confirm deletion? (y/n): ";
    cin >> confirm;

    if (confirm != 'y' && confirm != 'Y')
    {
        cout << "Deletion cancelled.\n";
        return;
    }

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "DELETE FROM rooms WHERE room_id = ?"
        )
    );

    stmt->setInt(1, id);

    int rows = stmt->executeUpdate();

    if (rows > 0)
        cout << "Room deleted successfully!\n";
    else
        cout << "Room not found.\n";
}

void roomManagementMenu(sql::Connection* con)
{
    int choice = 0;

    do
    {
        cout << "\n======= ROOM MANAGEMENT =======\n";
        cout << "1. Add Room\n";
        cout << "2. View All Rooms\n";
        cout << "3. Search Room by ID\n";
        cout << "4. Update Room\n";
        cout << "5. View Available Rooms\n";
        cout << "6. Delete Room\n";
        cout << "7. Back to Main Menu\n";
        cout << "Enter your choice: ";

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
                case 1: addRoom(con); break;
                case 2: viewRooms(con); break;
                case 3: searchRoom(con); break;
                case 4: updateRoom(con); break;
                case 5: viewAvailableRooms(con); break;
                case 6: deleteRoom(con); break;
                case 7: cout << "Returning to main menu...\n"; break;
                default: cout << "Invalid choice!\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cerr << "Database error: " << e.what() << "\n";
        }

    } while (choice != 7);
}
