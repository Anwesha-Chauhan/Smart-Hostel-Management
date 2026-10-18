#include "NoticeService.h"

#include <iostream>
#include <string>
#include <memory>
#include <limits>

#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>

using namespace std;

void noticeManagementMenu(sql::Connection* con)
{
    int choice = 0;

    do
    {
        cout << "\n===== NOTICE MANAGEMENT =====\n";
        cout << "1. Post Notice\n";
        cout << "2. View All Notices\n";
        cout << "3. Search Notice by ID\n";
        cout << "4. Search Notice by Title\n";
        cout << "5. Update Notice\n";
        cout << "6. Delete Notice\n";
        cout << "7. Back to Main Menu\n";
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
            // 1. Post Notice
            if (choice == 1)
            {
                int postedBy;
                string title, content;

                cout << "Enter Admin/Warden User ID: ";
                cin >> postedBy;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Enter Notice Title: ";
                getline(cin, title);

                cout << "Enter Notice Content: ";
                getline(cin, content);

                if (title.empty() || content.empty())
                {
                    cout << "Title and content cannot be empty.\n";
                    continue;
                }

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "INSERT INTO notices (title, content, posted_by) "
                        "VALUES (?, ?, ?)"
                    )
                );

                pstmt->setString(1, title);
                pstmt->setString(2, content);
                pstmt->setInt(3, postedBy);

                pstmt->executeUpdate();

                cout << "Notice posted successfully!\n";
            }

            // 2. View All Notices
            else if (choice == 2)
            {
                unique_ptr<sql::Statement> stmt(con->createStatement());

                unique_ptr<sql::ResultSet> res(
                    stmt->executeQuery(
                        "SELECT notice_id, title, content, posted_by, created_at "
                        "FROM notices ORDER BY created_at DESC"
                    )
                );

                bool found = false;

                while (res->next())
                {
                    found = true;

                    cout << "\n-----------------------------\n";
                    cout << "Notice ID : " << res->getInt("notice_id") << "\n";
                    cout << "Title     : " << res->getString("title") << "\n";
                    cout << "Content   : " << res->getString("content") << "\n";
                    cout << "Posted By : " << res->getInt("posted_by") << "\n";
                    cout << "Created At: " << res->getString("created_at") << "\n";
                }

                if (!found)
                    cout << "No notices found.\n";
            }

            // 3. Search Notice by ID
            else if (choice == 3)
            {
                int noticeId;

                cout << "Enter Notice ID: ";
                cin >> noticeId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "SELECT notice_id, title, content, posted_by, created_at "
                        "FROM notices WHERE notice_id = ?"
                    )
                );

                pstmt->setInt(1, noticeId);

                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

                if (res->next())
                {
                    cout << "\nNotice ID : " << res->getInt("notice_id") << "\n";
                    cout << "Title     : " << res->getString("title") << "\n";
                    cout << "Content   : " << res->getString("content") << "\n";
                    cout << "Posted By : " << res->getInt("posted_by") << "\n";
                    cout << "Created At: " << res->getString("created_at") << "\n";
                }
                else
                {
                    cout << "Notice not found.\n";
                }
            }

            // 4. Search Notice by Title
            else if (choice == 4)
            {
                string title;

                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Enter title keyword: ";
                getline(cin, title);

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "SELECT notice_id, title, content, posted_by, created_at "
                        "FROM notices WHERE title LIKE ? ORDER BY created_at DESC"
                    )
                );

                pstmt->setString(1, "%" + title + "%");

                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

                bool found = false;

                while (res->next())
                {
                    found = true;

                    cout << "\nNotice ID: " << res->getInt("notice_id") << "\n";
                    cout << "Title    : " << res->getString("title") << "\n";
                    cout << "Content  : " << res->getString("content") << "\n";
                    cout << "Posted By: " << res->getInt("posted_by") << "\n";
                }

                if (!found)
                    cout << "No matching notices found.\n";
            }

            // 5. Update Notice
            else if (choice == 5)
            {
                int noticeId;
                string title, content;

                cout << "Enter Notice ID to update: ";
                cin >> noticeId;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');

                cout << "Enter new title: ";
                getline(cin, title);

                cout << "Enter new content: ";
                getline(cin, content);

                if (title.empty() || content.empty())
                {
                    cout << "Title and content cannot be empty.\n";
                    continue;
                }

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "UPDATE notices SET title = ?, content = ? "
                        "WHERE notice_id = ?"
                    )
                );

                pstmt->setString(1, title);
                pstmt->setString(2, content);
                pstmt->setInt(3, noticeId);

                int rows = pstmt->executeUpdate();

                if (rows > 0)
                    cout << "Notice updated successfully!\n";
                else
                    cout << "Notice not found or no changes made.\n";
            }

            // 6. Delete Notice
            else if (choice == 6)
            {
                int noticeId;

                cout << "Enter Notice ID to delete: ";
                cin >> noticeId;

                unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "DELETE FROM notices WHERE notice_id = ?"
                    )
                );

                pstmt->setInt(1, noticeId);

                int rows = pstmt->executeUpdate();

                if (rows > 0)
                    cout << "Notice deleted successfully.\n";
                else
                    cout << "Notice not found.\n";
            }

            else if (choice == 7)
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
    while (choice != 7);
}
