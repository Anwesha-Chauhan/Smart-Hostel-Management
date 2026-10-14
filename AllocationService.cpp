
#include "AllocationService.h"

#include <iostream>
#include <memory>
#include <limits>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

using namespace std;

// 1. Allocate a room to a student
void allocateRoom(sql::Connection* con)
{
    int userId, roomId;

    cout << "\nEnter Student User ID: ";
    cin >> userId;

    cout << "Enter Room ID: ";
    cin >> roomId;

    try
    {
        con->setAutoCommit(false);

        // Check that the user exists and is a student
        unique_ptr<sql::PreparedStatement> userQuery(
            con->prepareStatement(
                "SELECT role FROM users WHERE user_id = ?"
            )
        );

        userQuery->setInt(1, userId);

        unique_ptr<sql::ResultSet> userResult(userQuery->executeQuery());

        if (!userResult->next() ||
            userResult->getString("role") != "Student")
        {
            cout << "Invalid student ID or user is not a student.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        // Prevent a student from having multiple active allocations
        unique_ptr<sql::PreparedStatement> existingQuery(
            con->prepareStatement(
                "SELECT allocation_id FROM room_allocations "
                "WHERE user_id = ? AND status = 'Active'"
            )
        );

        existingQuery->setInt(1, userId);

        unique_ptr<sql::ResultSet> existingResult(
            existingQuery->executeQuery()
        );

        if (existingResult->next())
        {
            cout << "This student already has an active room allocation.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        // Lock the room row to prevent concurrent over-allocation
        unique_ptr<sql::PreparedStatement> roomQuery(
            con->prepareStatement(
                "SELECT capacity, current_occupancy, status "
                "FROM rooms WHERE room_id = ? FOR UPDATE"
            )
        );

        roomQuery->setInt(1, roomId);

        unique_ptr<sql::ResultSet> roomResult(roomQuery->executeQuery());

        if (!roomResult->next())
        {
            cout << "Room not found.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        int capacity = roomResult->getInt("capacity");
        int occupancy = roomResult->getInt("current_occupancy");
        string status = roomResult->getString("status");

        if (status == "Maintenance" || occupancy >= capacity)
        {
            cout << "Room is unavailable or full.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        // Create the allocation
        unique_ptr<sql::PreparedStatement> insertQuery(
            con->prepareStatement(
                "INSERT INTO room_allocations "
                "(user_id, room_id, allocation_date, status) "
                "VALUES (?, ?, CURRENT_DATE, 'Active')"
            )
        );

        insertQuery->setInt(1, userId);
        insertQuery->setInt(2, roomId);
        insertQuery->executeUpdate();

        // Update occupancy and room status
        unique_ptr<sql::PreparedStatement> updateRoom(
            con->prepareStatement(
                "UPDATE rooms SET "
                "current_occupancy = current_occupancy + 1, "
                "status = CASE "
                "WHEN current_occupancy + 1 >= capacity "
                "THEN 'Full' ELSE 'Available' END "
                "WHERE room_id = ?"
            )
        );

        updateRoom->setInt(1, roomId);
        updateRoom->executeUpdate();

        con->commit();
        con->setAutoCommit(true);

        cout << "Room allocated successfully!\n";
    }
    catch (sql::SQLException& e)
    {
        try
        {
            con->rollback();
            con->setAutoCommit(true);
        }
        catch (...) {}

        cout << "Allocation failed: " << e.what() << "\n";
    }
}

// 2. View all room allocations
void viewAllocations(sql::Connection* con)
{
    unique_ptr<sql::Statement> stmt(con->createStatement());

    unique_ptr<sql::ResultSet> res(
        stmt->executeQuery(
            "SELECT a.allocation_id, a.user_id, u.name, "
            "a.room_id, r.room_number, a.allocation_date, a.status "
            "FROM room_allocations a "
            "JOIN users u ON a.user_id = u.user_id "
            "JOIN rooms r ON a.room_id = r.room_id "
            "ORDER BY a.allocation_id"
        )
    );

    cout << "\n--- Room Allocations ---\n";

    while (res->next())
    {
        cout << "\nAllocation ID: "
             << res->getInt("allocation_id")
             << "\nStudent ID: " << res->getInt("user_id")
             << "\nStudent Name: " << res->getString("name")
             << "\nRoom ID: " << res->getInt("room_id")
             << "\nRoom Number: " << res->getString("room_number")
             << "\nAllocation Date: " << res->getString("allocation_date")
             << "\nStatus: " << res->getString("status")
             << "\n-------------------------\n";
    }
}

// 3. Search allocations by student ID
void searchAllocation(sql::Connection* con)
{
    int userId;

    cout << "Enter Student User ID: ";
    cin >> userId;

    unique_ptr<sql::PreparedStatement> stmt(
        con->prepareStatement(
            "SELECT allocation_id, room_id, allocation_date, status "
            "FROM room_allocations WHERE user_id = ?"
        )
    );

    stmt->setInt(1, userId);

    unique_ptr<sql::ResultSet> res(stmt->executeQuery());

    if (!res->next())
    {
        cout << "No allocations found for this student.\n";
        return;
    }

    do
    {
        cout << "\nAllocation ID: " << res->getInt("allocation_id")
             << "\nRoom ID: " << res->getInt("room_id")
             << "\nDate: " << res->getString("allocation_date")
             << "\nStatus: " << res->getString("status") << "\n";
    } while (res->next());
}

// 4. Vacate a room
void vacateRoom(sql::Connection* con)
{
    int allocationId;

    cout << "Enter Allocation ID to vacate: ";
    cin >> allocationId;

    try
    {
        con->setAutoCommit(false);

        // Lock the allocation row
        unique_ptr<sql::PreparedStatement> allocationQuery(
            con->prepareStatement(
                "SELECT user_id, room_id, status "
                "FROM room_allocations "
                "WHERE allocation_id = ? FOR UPDATE"
            )
        );

        allocationQuery->setInt(1, allocationId);

        unique_ptr<sql::ResultSet> allocationResult(
            allocationQuery->executeQuery()
        );

        if (!allocationResult->next())
        {
            cout << "Allocation not found.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        int roomId = allocationResult->getInt("room_id");
        string status = allocationResult->getString("status");

        if (status != "Active")
        {
            cout << "This allocation is already vacated.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        // Lock the room row
        unique_ptr<sql::PreparedStatement> roomQuery(
            con->prepareStatement(
                "SELECT current_occupancy FROM rooms "
                "WHERE room_id = ? FOR UPDATE"
            )
        );

        roomQuery->setInt(1, roomId);

        unique_ptr<sql::ResultSet> roomResult(roomQuery->executeQuery());

        if (!roomResult->next() ||
            roomResult->getInt("current_occupancy") <= 0)
        {
            cout << "Room occupancy is inconsistent. Please check the database.\n";
            con->rollback();
            con->setAutoCommit(true);
            return;
        }

        // Mark allocation as vacated
        unique_ptr<sql::PreparedStatement> updateAllocation(
            con->prepareStatement(
                "UPDATE room_allocations "
                "SET status = 'Vacated' WHERE allocation_id = ?"
            )
        );

        updateAllocation->setInt(1, allocationId);
        updateAllocation->executeUpdate();

        // Decrease occupancy and refresh room status
        unique_ptr<sql::PreparedStatement> updateRoom(
            con->prepareStatement(
                "UPDATE rooms SET "
                "current_occupancy = current_occupancy - 1, "
                "status = CASE "
                "WHEN current_occupancy - 1 = 0 THEN 'Available' "
                "WHEN current_occupancy - 1 < capacity THEN 'Available' "
                "ELSE 'Full' END "
                "WHERE room_id = ?"
            )
        );

        updateRoom->setInt(1, roomId);
        updateRoom->executeUpdate();

        con->commit();
        con->setAutoCommit(true);

        cout << "Room vacated successfully!\n";
    }
    catch (sql::SQLException& e)
    {
        try
        {
            con->rollback();
            con->setAutoCommit(true);
        }
        catch (...) {}

        cout << "Vacating failed: " << e.what() << "\n";
    }
}

// Main allocation menu
void allocationManagementMenu(sql::Connection* con)
{
    int choice;

    do
    {
        cout << "\n===== ROOM ALLOCATION =====\n";
        cout << "1. Allocate Room\n";
        cout << "2. View All Allocations\n";
        cout << "3. Search by Student ID\n";
        cout << "4. Vacate Room\n";
        cout << "5. Back to Main Menu\n";
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
                    allocateRoom(con);
                    break;

                case 2:
                    viewAllocations(con);
                    break;

                case 3:
                    searchAllocation(con);
                    break;

                case 4:
                    vacateRoom(con);
                    break;

                case 5:
                    break;

                default:
                    cout << "Invalid choice.\n";
            }
        }
        catch (sql::SQLException& e)
        {
            cout << "Database error: " << e.what() << "\n";
        }

    } while (choice != 5);
}
