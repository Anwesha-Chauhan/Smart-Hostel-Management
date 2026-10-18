# Smart Hostel Management System

A database-driven hostel management application developed using **C++, MySQL, and MySQL Connector/C++** as part of the OSDBMS (Operating Systems and Database Management Systems) project.

The system helps manage hostel operations such as student records, room allocation, fee payments, complaints, leave requests, visitors, attendance, and notices through a menu-driven interface.

## Project Information

- **Project Name:** Smart Hostel Management System
- **Team Name:** RamdomRockstars
- **Project Code:** OSDBMS-V-2026-T232
- **Domain:** Operating Systems and Database Management Systems
- **Programming Language:** C++
- **Database:** MySQL
- **Database Connectivity:** MySQL Connector/C++
- **Development Environment:** Visual Studio Code, CMake, and MSVC Build Tools

## Features and Modules

The project consists of nine modules:

### 1. User Management
- Add new users and view existing user records.
- Search users by their details.
- Update user information.
- Delete user records.
- Maintain user roles such as Student, Warden, Maintenance, and Admin.

### 2. Room Management
- Add and view hostel rooms.
- Search rooms by room details.
- Update room information and capacity.
- Check room availability.
- Delete room records when permitted.

### 3. Room Allocation
- Allocate rooms to students.
- View and search room allocation records.
- Validate room capacity before allocation.
- Vacate allocated rooms.
- Use database transactions to help maintain consistent allocation data.

### 4. Fee Management
- Add hostel fee records.
- View and search fee details.
- Record fee payments.
- Track pending and overdue fees.
- Maintain payment status.

### 5. Complaint Management
- Register and view complaints.
- Search complaints by student ID.
- Update complaint status.
- Assign complaints to staff.
- Delete complaint records when required.

### 6. Leave Management
- Submit leave requests.
- View and search leave records.
- Approve or reject requests.
- Delete leave records when required.
- Maintain leave request status.

### 7. Visitor Management
- Register hostel visitors.
- View all visitor records.
- Search visitors by student ID.
- Record visitor check-out.
- Delete visitor records when required.

### 8. Attendance Management
- Record student attendance.
- View all attendance records.
- Search attendance by user ID.
- Delete attendance records when required.
- Maintain attendance status and date.

### 9. Notice Management
- Post hostel notices.
- View all notices.
- Search notices by ID or title keyword.
- Update notice titles and content.
- Delete notices when required.

## Technologies Used

| Technology | Purpose |
|---|---|
| C++ | Application logic and menu-driven interface |
| MySQL | Relational database management |
| MySQL Connector/C++ | Connectivity between C++ and MySQL |
| SQL | Database operations and data manipulation |
| CMake | Project configuration and build management |
| Visual Studio Code | Code editing and development |
| MSVC Build Tools | C++ compilation |

## Database Design

The project uses a MySQL database named `smart_hostel_db`.

The database contains the following main tables:

- `users` — user details, roles, and password storage
- `rooms` — room details, capacity, occupancy, and status
- `room_allocations` — room allocation and vacating records
- `fees` — fee amounts, due dates, payment dates, and statuses
- `complaints` — complaints, assigned staff, and resolution status
- `leave_requests` — leave applications and approval status
- `visitors` — visitor details and check-in/check-out records
- `attendance` — attendance dates and statuses
- `notices` — hostel announcements and notice details

The database uses primary keys, foreign keys, unique constraints, `NOT NULL` constraints, default values, and other integrity rules where appropriate.

## Operating Systems Concepts

The project also demonstrates selected Operating Systems concepts:

- **Process Scheduling:** FCFS and Priority Scheduling for request-processing demonstrations.
- **Synchronization and Concurrency:** Database transactions and validation for safer room allocation.
- **Resource Management:** Room capacity and occupancy tracking.
- **Modular Design:** Separate service modules for different hostel operations.

## Prerequisites

Before building the project, ensure that you have:

1. Windows operating system.
2. Visual Studio Code or another compatible code editor.
3. Microsoft C++ Build Tools with the MSVC compiler.
4. CMake installed.
5. MySQL Server and MySQL Workbench.
6. MySQL Connector/C++ installed.

## Database Setup

1. Start MySQL Server.
2. Open MySQL Workbench.
3. Create the database:

   ```sql
   CREATE DATABASE smart_hostel_db;
   USE smart_hostel_db;
   ```

4. Create the required tables using the project's SQL schema.
5. Configure the database connection in the C++ database connection code.

Make sure the MySQL hostname, port, username, and password match your local MySQL configuration.

**Important:** Never commit real database passwords or private credentials to a public repository.

## Build and Run

Open a Developer Command Prompt configured for the x64 MSVC compiler.

Navigate to the project directory:

```bat
cd /d "C:\Users\Dell\Desktop\OSDBMS project"
```

Configure the project with CMake:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Program Files/MySQL/MySQL Connector C++ 26.7"
```

Build the project:

```bat
cmake --build build --config Release
```

Add the Connector/C++ library directories to the runtime path:

```bat
set "PATH=C:\Program Files\MySQL\MySQL Connector C++ 26.7\lib64;C:\Program Files\MySQL\MySQL Connector C++ 26.7\bin;%PATH%"
```

Run the application:

```bat
build\Release\hostel_db_test.exe
```

**Note:** Update the project path, connector path, and executable name if your local installation uses different locations.

## Project Structure

The project is organized into separate service modules to improve readability and maintainability.

```text
OSDBMS project/
├── main.cpp
├── CMakeLists.txt
├── Database.h
├── Database.cpp
├── UserService.h
├── UserService.cpp
├── RoomService.h
├── RoomService.cpp
├── AllocationService.h
├── AllocationService.cpp
├── FeeService.h
├── FeeService.cpp
├── ComplaintService.h
├── ComplaintService.cpp
├── LeaveService.h
├── LeaveService.cpp
├── VisitorService.h
├── VisitorService.cpp
├── AttendanceService.h
├── AttendanceService.cpp
├── NoticeService.h
├── NoticeService.cpp
└── build/
```

*The exact file and folder arrangement may differ depending on the project's current CMake configuration.*

## Testing

The system can be tested using the following scenarios:

- Adding, searching, updating, and deleting user records.
- Checking room capacity and room availability.
- Allocating rooms and vacating them.
- Recording fee payments and checking outstanding fees.
- Creating complaints and changing complaint status.
- Applying for leave and processing requests.
- Registering visitors and recording check-out.
- Recording and searching attendance.
- Creating, searching, updating, and deleting notices.
- Testing invalid inputs and database constraints.

## Future Enhancements

Possible future improvements include:

- Role-Based Access Control for Admin, Warden, and Maintenance Staff.
- Secure login and password hashing.
- A graphical web-based user interface.
- Student dashboards and self-service features.
- Automated notifications for fees, leave requests, and complaints.
- Reports and analytics for hostel administration.
- Improved audit logging and security controls.

These are proposed enhancements and should not be considered implemented unless they have been completed and tested.

## Learning Outcomes

Through this project, the team gains practical experience in:

- C++ modular programming.
- MySQL database design and SQL queries.
- Database connectivity using MySQL Connector/C++.
- Relational integrity and transaction handling.
- CRUD operations and exception handling.
- Applying selected Operating Systems concepts to a real-world management scenario.
- Building and maintaining a multi-module software application.

## Team

**Team Name:** RamdomRockstars

- **Akanksha** — Team Lead
- **Anwesha Chauhan** — Team Member
- **Vansh Aswal** — Team Member

## Conclusion

The Smart Hostel Management System provides a structured approach to managing common hostel operations through a modular C++ application connected to a MySQL database. It combines database management, data validation, transaction handling, and selected Operating Systems concepts in a single academic project.

