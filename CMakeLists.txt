
cmake_minimum_required(VERSION 3.20)

project(SmartHostelManagement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mysql-concpp CONFIG REQUIRED)

add_executable(hostel_db_test
    main.cpp
    database/Database.cpp
    modules/users/UserService.cpp
    modules/users/AuthService.cpp
    modules/rooms/RoomService.cpp
    modules/allocations/AllocationService.cpp
    modules/fees/FeeService.cpp
    modules/complaints/ComplaintService.cpp
    modules/leave/LeaveService.cpp
    modules/attendence/AttendenceService.cpp
    modules/visitors/VisitorService.cpp
    modules/notices/NoticeService.cpp
    
)

target_include_directories(hostel_db_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/database"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/users"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/rooms"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/allocations"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/fees"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/complaints"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/leave"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/attendence"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/visitors"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/notices"
    
)

target_link_libraries(hostel_db_test
    PRIVATE
        mysql::concpp-jdbc
        bcrypt
)

add_executable(hostel_web_server
    server.cpp
    database/Database.cpp
    modules/users/AuthService.cpp
    modules/users/UserWebService.cpp
    modules/rooms/RoomWebService.cpp
    modules/allocations/AllocationWebService.cpp
    modules/fees/FeeWebService.cpp
    modules/leave/LeaveWebService.cpp
    modules/visitors/VisitorWebService.cpp
    modules/complaints/ComplaintWebService.cpp
)

target_include_directories(hostel_web_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/database"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/users"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/rooms"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/allocations"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/fees"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/leave"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/visitors"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/complaints"
    "C:/Program Files/MySQL/MySQL Connector C++ 26.7/include"
)

target_link_libraries(hostel_web_server PRIVATE
    "C:/Program Files/MySQL/MySQL Connector C++ 26.7/lib64/vs14/mysqlcppconn.lib"
    bcrypt
)

