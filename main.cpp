
#include <iostream>
#include <memory>

#include "Database.h"
#include "UserService.h"
#include "RoomService.h"
#include "AllocationService.h"
#include "FeeService.h"
#include "ComplaintService.h"
#include "LeaveService.h"
#include "VisitorService.h"
#include "AttendenceService.h"
#include "NoticeService.h"

using namespace std;

int main()
{
    try
    {
        unique_ptr<sql::Connection> con = connectDatabase();

        cout << "Database connected successfully!\n";

        int choice = 0;

        do
        {
            cout << "\n================================\n";
            cout << " SMART HOSTEL MANAGEMENT SYSTEM\n";
            cout << "================================\n";
            cout << "1. User Management\n";
            cout << "2. Room Management\n";
            cout << "3. Room Allocation\n";
            cout << "4. Fee Management\n";
            cout << "5. Complaint Management\n";
            cout << "6. Leave Management\n";
            cout << "7. Visitor Management\n";
            cout << "8. Attendance Management\n";
            cout << "9. Notice Management\n";
            cout << "10. Exit\n";
            cout << "Enter your choice: ";

            if (!(cin >> choice))
            {
                cout << "Please enter a number.\n";
                cin.clear();
                cin.ignore(10000, '\n');
                continue;
            }

            switch (choice)
            {
                case 1:
                    userManagementMenu(con.get());
                    break;

                case 2:
                    roomManagementMenu(con.get());
                    break;

                case 3:
                    allocationManagementMenu(con.get());
                    break;

                case 4:
                    feeManagementMenu(con.get());
                    break;

                case 5:
                    complaintManagementMenu(con.get());
                    break;

                case 6:
                    leaveManagementMenu(con.get());
                    break;

                case 7:
                    visitorManagementMenu(con.get());
                    break;

                case 8:
                    attendenceManagementMenu(con.get());
                    break;

                case 9:
                    noticeManagementMenu(con.get());
                    break;

                case 10:
                    cout << "Thank you for using Smart Hostel Management System!\n";
                    break;

                default:
                    cout << "Invalid choice. Please try again.\n";
            }

        } while (choice != 10);
    }
    catch (sql::SQLException& e)
    {
        cerr << "Database error: " << e.what() << "\n";
        return 1;
    }
    catch (exception& e)
    {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
