
cmake_minimum_required(VERSION 3.20)

project(SmartHostelManagement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(mysql-concpp CONFIG REQUIRED)

add_executable(hostel_db_test
    main.cpp
    database/Database.cpp
    modules/users/UserService.cpp
    modules/rooms/RoomService.cpp
    modules/allocations/AllocationService.cpp
    modules/fees/FeeService.cpp
    modules/complaints/ComplaintService.cpp
)

target_include_directories(hostel_db_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/database"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/users"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/rooms"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/allocations"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/fees"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/complaints"
)

target_link_libraries(hostel_db_test
    PRIVATE mysql::concpp-jdbc
)
