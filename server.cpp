
#include <stdexcept>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>
#include <iomanip>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <Windows.h>
#include <bcrypt.h>

#include "httplib.h"
#include "Database.h"
#include "AuthService.h"
#include "UserWebService.h"
#include "RoomWebService.h"
#include "AllocationWebService.h"
#include "FeeWebService.h"
#include "LeaveWebService.h"
#include "VisitorWebService.h"
#include "ComplaintWebService.h"

#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/prepared_statement.h>

// -----------------------------------------
// SESSION CONFIGURATION
// -----------------------------------------

const int SESSION_DURATION_MINUTES = 30;

struct Session
{
    int userId;
    std::string email;
    std::string role;
    std::chrono::steady_clock::time_point expiry;
};
std::unordered_map<std::string, Session> sessions;
std::mutex sessionsMutex;

// -----------------------------------------
// SESSION TOKEN
// -----------------------------------------

std::string generateSessionToken()
{
    unsigned char bytes[32];

    NTSTATUS status = BCryptGenRandom(
        nullptr,
        bytes,
        sizeof(bytes),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (status != 0)
        throw std::runtime_error("Could not generate secure session token.");

    std::ostringstream token;
    token << std::hex << std::setfill('0');

    for (unsigned char byte : bytes)
        token << std::setw(2) << static_cast<int>(byte);

    return token.str();
}

std::string getSessionToken(const httplib::Request& req)
{
    std::string cookie = req.get_header_value("Cookie");
    std::stringstream stream(cookie);
    std::string item;

    while (std::getline(stream, item, ';'))
    {
        size_t start = item.find_first_not_of(" \t");

        if (start != std::string::npos)
            item = item.substr(start);

        const std::string prefix = "SHMS_SESSION=";

        if (item.rfind(prefix, 0) == 0)
            return item.substr(prefix.length());
    }

    return "";
}

bool isValidSession(
    const httplib::Request& req,
    Session& currentSession
)
{
    std::string token = getSessionToken(req);

    if (token.empty())
        return false;

    std::lock_guard<std::mutex> lock(sessionsMutex);

    auto it = sessions.find(token);

    if (it == sessions.end())
        return false;

    if (std::chrono::steady_clock::now() >= it->second.expiry)
    {
        sessions.erase(it);
        return false;
    }

    currentSession = it->second;
    return true;
}

// -----------------------------------------
// COMMON HELPERS
// -----------------------------------------

bool serveFile(
    const std::string& path,
    const std::string& contentType,
    httplib::Response& res
)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        res.status = 404;
        res.set_content("File not found.", "text/plain");
        return false;
    }

    std::ostringstream content;
    content << file.rdbuf();

    res.set_content(content.str(), contentType);
    return true;
}

std::string getDashboardForRole(const std::string& role)
{
    if (role == "Admin") return "/index.html";
    if (role == "Warden") return "/dashboards/warden.html";
    if (role == "Student") return "/dashboards/student.html";
    if (role == "Maintenance") return "/dashboards/maintenance.html";

    return "";
}

std::string jsonEscape(const std::string& value)
{
    std::string output;

    for (unsigned char c : value)
    {
        switch (c)
        {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;

            default:
                if (c < 0x20)
                {
                    const char hex[] = "0123456789abcdef";
                    output += "\\u00";
                    output += hex[(c >> 4) & 0x0F];
                    output += hex[c & 0x0F];
                }
                else
                {
                    output += static_cast<char>(c);
                }
        }
    }

    return output;
}

void sendApiError(
    httplib::Response& res,
    int status,
    const std::string& message
)
{
    res.status = status;

    res.set_content(
        "{\"success\":false,\"message\":\"" +
        jsonEscape(message) + "\"}",
        "application/json"
    );
}

// Authorization for normal frontend pages.
bool authorizeRole(
    const httplib::Request& req,
    httplib::Response& res,
    const std::vector<std::string>& allowedRoles
)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        res.status = 302;
        res.set_header("Location", "/login.html");
        return false;
    }

    for (const std::string& role : allowedRoles)
    {
        if (currentSession.role == role)
            return true;
    }

    res.status = 403;
    res.set_content(
        "Access denied. You do not have permission to access this page.",
        "text/plain"
    );

    return false;
}

// Authorization for APIs. Returns JSON errors instead of redirects.
bool authorizeApiRole(
    const httplib::Request& req,
    httplib::Response& res,
    const std::vector<std::string>& allowedRoles
)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return false;
    }

    for (const std::string& role : allowedRoles)
    {
        if (currentSession.role == role)
            return true;
    }

    sendApiError(res, 403, "You do not have permission to perform this action.");
    return false;
}

// -----------------------------------------
// MAIN SERVER
// -----------------------------------------

int main()
{
    httplib::Server server;

    // -------------------------------------
    // LOGIN PAGE
    // -------------------------------------

    server.Get("/", [](const httplib::Request&, httplib::Response& res)
    {
        res.status = 302;
        res.set_header("Location", "/login.html");
    });

    server.Get("/login.html", [](const httplib::Request&, httplib::Response& res)
    {
        serveFile("./frontend/login.html", "text/html", res);
    });

    // -------------------------------------
    // LOGIN API
    // -------------------------------------

    server.Post("/api/login", [](const httplib::Request& req, httplib::Response& res)
    {
        try
        {
            std::string email = req.get_param_value("email");
            std::string password = req.get_param_value("password");

            if (email.empty() || password.empty())
            {
                sendApiError(res, 400, "Email and password are required.");
                return;
            }

            auto con = connectDatabase();
            AuthenticatedUser user{};

            if (!authenticateUser(con.get(), email, password, user))
            {
                sendApiError(res, 401, "Invalid email or password.");
                return;
            }

            std::string dashboard = getDashboardForRole(user.role);

            if (dashboard.empty())
            {
                sendApiError(res, 403, "Invalid user role.");
                return;
            }

            std::string token = generateSessionToken();

           Session newSession;
            newSession.userId = user.userId;
            newSession.email = user.email;
            newSession.role = user.role;
            newSession.expiry =
                std::chrono::steady_clock::now() +
                std::chrono::minutes(SESSION_DURATION_MINUTES);

            {
                std::lock_guard<std::mutex> lock(sessionsMutex);
                sessions[token] = newSession;
            }

            res.set_header(
                "Set-Cookie",
                "SHMS_SESSION=" + token +
                "; HttpOnly; SameSite=Strict; Path=/; Max-Age=1800"
            );

            res.set_header("Cache-Control", "no-store");

            std::cout << "Login successful: "
                      << user.email << " (" << user.role << ")\n";

            res.set_content(
                "{\"success\":true,"
                "\"message\":\"Login successful.\","
                "\"redirect\":\"" + dashboard + "\"}",
                "application/json"
            );
        }
        catch (const std::exception& e)
        {
            std::cerr << "Login error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error. Please try again.");
        }
    });

    // -----------------------------------------
// CURRENT SESSION API
// -----------------------------------------

server.Get("/api/session",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    res.set_header("Cache-Control", "no-store");

    res.set_content(
    "{\"success\":true,"
    "\"userId\":" + std::to_string(currentSession.userId) +
    ",\"role\":\"" + jsonEscape(currentSession.role) + "\"}",
    "application/json"
);
});

    // -------------------------------------
    // DASHBOARDS
    // -------------------------------------

    server.Get("/index.html", [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Admin"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/index.html", "text/html", res);
    });

    server.Get("/dashboards/warden.html",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Warden"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/dashboards/warden.html", "text/html", res);
    });

    server.Get("/dashboards/student.html",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Student"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/dashboards/student.html", "text/html", res);
    });

    server.Get("/dashboards/maintenance.html",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Maintenance"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/dashboards/maintenance.html", "text/html", res);
    });

    // -------------------------------------
    // FRONTEND PAGES
    // -------------------------------------

    server.Get("/pages/students.html",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Admin"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/pages/students.html", "text/html", res);
    });

    server.Get("/pages/rooms.html",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeRole(req, res, {"Admin", "Warden", "Student"})) return;

        res.set_header("Cache-Control", "no-store");
        serveFile("./frontend/pages/rooms.html", "text/html", res);
    });

    // ALLOCATION PAGE
server.Get("/pages/allocation.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Admin", "Warden", "Student"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/allocation.html", "text/html", res);
});


server.Get("/pages/availability.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Admin"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/availability.html", "text/html", res);
});

// LEAVE MANAGEMENT PAGE - WARDEN ONLY
server.Get("/pages/leave-management.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Warden"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/leave-management.html", "text/html", res);
});

// VISITOR MANAGEMENT PAGE - WARDEN ONLY
server.Get("/pages/visitor-management.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Warden"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/visitor-management.html", "text/html", res);
});


server.Get("/pages/complaint-management.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Warden"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/complaint-management.html", "text/html", res);
});


// FEE MANAGEMENT PAGE - ADMIN ONLY
server.Get("/pages/fees.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Admin"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/fees.html", "text/html", res);
});

// STUDENT FEES PAGE - STUDENT ONLY
server.Get("/pages/my-fees.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Student"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/my-fees.html", "text/html", res);
});

// STUDENT LEAVE APPLICATION PAGE
server.Get("/pages/apply-leave.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Student"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/apply-leave.html", "text/html", res);
});

// STUDENT VISITOR HISTORY PAGE
server.Get("/pages/my-visitors.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Student"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/my-visitors.html", "text/html", res);
});


server.Get("/pages/complaint-submission.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Student"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/complaint-submission.html", "text/html", res);
});


server.Get("/pages/maintenance-complaints.html",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeRole(req, res, {"Maintenance"}))
        return;

    res.set_header("Cache-Control", "no-store");
    serveFile("./frontend/pages/maintenance-complaints.html", "text/html", res);
});


    // -------------------------------------
    // ADMIN USER MANAGEMENT API
    // -------------------------------------

    // GET ALL USERS
    server.Get("/api/admin/users",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            auto con = connectDatabase();
            std::vector<WebUser> users = getAllWebUsers(con.get());

            std::string json = "{\"success\":true,\"users\":[";

            for (size_t i = 0; i < users.size(); i++)
            {
                if (i > 0) json += ",";

                const auto& user = users[i];

                json +=
                    "{\"userId\":" + std::to_string(user.userId) +
                    ",\"name\":\"" + jsonEscape(user.name) +
                    "\",\"email\":\"" + jsonEscape(user.email) +
                    "\",\"role\":\"" + jsonEscape(user.role) +
                    "\",\"phone\":\"" + jsonEscape(user.phone) + "\"}";
            }

            json += "]}";

            res.set_header("Cache-Control", "no-store");
            res.set_content(json, "application/json");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get users error: " << e.what() << "\n";
            sendApiError(res, 500, "Could not retrieve users.");
        }
    });

    // ADD USER
    server.Post("/api/admin/users",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            auto con = connectDatabase();

            bool success = createWebUser(
                con.get(),
                req.get_param_value("name"),
                req.get_param_value("email"),
                req.get_param_value("password"),
                req.get_param_value("role"),
                req.get_param_value("phone")
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"User added successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 400, "User could not be added.");
            }
        }
        catch (const std::invalid_argument& e)
        {
            sendApiError(res, 400, e.what());
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Add user SQL error: " << e.what() << "\n";
            sendApiError(res, 409, "Could not add user. Check whether the email already exists.");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Add user error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error while adding user.");
        }
    });

    // UPDATE USER
    server.Put(R"(/api/admin/users/(\d+))",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            int userId = std::stoi(req.matches[1].str());
            auto con = connectDatabase();

            bool success = updateWebUser(
                con.get(),
                userId,
                req.get_param_value("name"),
                req.get_param_value("email"),
                req.get_param_value("password"),
                req.get_param_value("role"),
                req.get_param_value("phone")
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"User updated successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 404, "User not found.");
            }
        }
        catch (const std::invalid_argument& e)
        {
            sendApiError(res, 400, e.what());
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Update user SQL error: " << e.what() << "\n";
            sendApiError(res, 409, "Could not update user. Check whether the email already exists.");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Update user error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error while updating user.");
        }
    });

    // DELETE USER
    server.Delete(R"(/api/admin/users/(\d+))",
        [](const httplib::Request& req, httplib::Response& res)
    {
        Session currentSession;

        if (!isValidSession(req, currentSession))
        {
            sendApiError(res, 401, "Please log in again.");
            return;
        }

        if (currentSession.role != "Admin")
        {
            sendApiError(res, 403, "Admin access required.");
            return;
        }

        try
        {
            int userId = std::stoi(req.matches[1].str());
            auto con = connectDatabase();

            if (deleteWebUser(con.get(), userId, currentSession.email))
            {
                res.set_content(
                    R"({"success":true,"message":"User deleted successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 404, "User not found.");
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Delete user error: " << e.what() << "\n";
            sendApiError(res, 400, e.what());
        }
    });

    // ================= STUDENT FEES API =================

server.Get("/api/student/fees",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    // Validate login session
    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    // Only students can access their own fee records
    if (currentSession.role != "Student")
    {
        sendApiError(res, 403, "Access denied.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        // Fetch fees using the logged-in student's ID
        std::vector<WebFee> fees =
            getWebFees(con.get(), currentSession.userId);

        std::string json = "{\"success\":true,\"fees\":[";

        for (size_t i = 0; i < fees.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& fee = fees[i];

            json +=
                "{\"feeId\":" + std::to_string(fee.feeId) +
                ",\"userId\":" + std::to_string(fee.userId) +
                ",\"amount\":" + std::to_string(fee.amount) +
                ",\"dueDate\":\"" + jsonEscape(fee.dueDate) +
                "\",\"paymentDate\":\"" + jsonEscape(fee.paymentDate) +
                "\",\"status\":\"" + jsonEscape(fee.status) + "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get student fees error: "
                  << e.what() << "\n";

        sendApiError(res, 500, "Could not retrieve your fee records.");
    }
});

// ================= LEAVE REQUESTS API =================

server.Get("/api/leave/requests",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Student" &&
        currentSession.role != "Warden")
    {
        sendApiError(res, 403, "Access denied.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        int userId = (currentSession.role == "Student")
                     ? currentSession.userId : 0;

        std::vector<WebLeaveRequest> requests =
            getWebLeaveRequests(con.get(), userId);

        std::string json = "{\"success\":true,\"requests\":[";

        for (size_t i = 0; i < requests.size(); i++)
        {
            if (i > 0) json += ",";

            const auto& leave = requests[i];

            json +=
                "{\"leaveId\":" + std::to_string(leave.leaveId) +
                ",\"userId\":" + std::to_string(leave.userId) +
                ",\"studentName\":\"" + jsonEscape(leave.studentName) +
                "\",\"fromDate\":\"" + jsonEscape(leave.fromDate) +
                "\",\"toDate\":\"" + jsonEscape(leave.toDate) +
                "\",\"reason\":\"" + jsonEscape(leave.reason) +
                "\",\"status\":\"" + jsonEscape(leave.status) + "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get leave requests error: "
                  << e.what() << "\n";

        sendApiError(res, 500, "Could not retrieve leave requests.");
    }
});

// ================= APPLY FOR LEAVE API =================

server.Post("/api/leave/apply",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Student")
    {
        sendApiError(res, 403, "Only students can apply for leave.");
        return;
    }

    if (!req.has_param("fromDate") ||
        !req.has_param("toDate") ||
        !req.has_param("reason"))
    {
        sendApiError(res, 400, "Please fill in all leave details.");
        return;
    }

    std::string fromDate = req.get_param_value("fromDate");
    std::string toDate = req.get_param_value("toDate");
    std::string reason = req.get_param_value("reason");

    if (fromDate.empty() || toDate.empty() || reason.empty())
    {
        sendApiError(res, 400, "All fields are required.");
        return;
    }

    if (fromDate.size() != 10 || toDate.size() != 10)
    {
        sendApiError(res, 400, "Invalid date format.");
        return;
    }

    if (fromDate > toDate)
    {
        sendApiError(res, 400, "From date cannot be after To date.");
        return;
    }

    if (reason.size() > 1000)
    {
        sendApiError(res, 400, "Reason is too long.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        bool success = addWebLeaveRequest(
            con.get(),
            currentSession.userId,
            fromDate,
            toDate,
            reason
        );

        if (!success)
        {
            sendApiError(res, 400, "Could not submit leave request.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Leave request submitted successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Apply leave error: " << e.what() << "\n";
        sendApiError(res, 500, "Could not submit leave request.");
    }
});

// ================= UPDATE LEAVE STATUS API =================

server.Post("/api/leave/update",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Warden")
    {
        sendApiError(res, 403, "Only the Warden can update leave requests.");
        return;
    }

    if (!req.has_param("leaveId") ||
        !req.has_param("status"))
    {
        sendApiError(res, 400, "Leave ID and status are required.");
        return;
    }

    int leaveId = 0;

    try
    {
        std::string id = req.get_param_value("leaveId");

        if (id.empty() ||
            id.find_first_not_of("0123456789") != std::string::npos)
        {
            sendApiError(res, 400, "Invalid leave ID.");
            return;
        }

        leaveId = std::stoi(id);
    }
    catch (...)
    {
        sendApiError(res, 400, "Invalid leave ID.");
        return;
    }

    std::string status = req.get_param_value("status");

    if (status != "Approved" && status != "Rejected")
    {
        sendApiError(res, 400, "Invalid leave status.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        bool success = updateWebLeaveStatus(
            con.get(),
            leaveId,
            status
        );

        if (!success)
        {
            sendApiError(
                res, 404,
                "Leave request not found or already processed."
            );
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Leave status updated successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Update leave error: " << e.what() << "\n";
        sendApiError(res, 500, "Could not update leave status.");
    }
});


// ================= VISITOR MANAGEMENT APIs =================

// 1. GET ALL VISITORS - WARDEN ONLY
server.Get("/api/visitors",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    try
    {
        auto con = connectDatabase();
        std::vector<WebVisitor> visitors = getWebVisitors(con.get());

        std::string json = "{\"success\":true,\"visitors\":[";

        for (size_t i = 0; i < visitors.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& v = visitors[i];

            json +=
                "{\"visitorId\":" + std::to_string(v.visitorId) +
                ",\"studentId\":" + std::to_string(v.studentId) +
                ",\"studentName\":\"" + jsonEscape(v.studentName) +
                "\",\"visitorName\":\"" + jsonEscape(v.visitorName) +
                "\",\"relation\":\"" + jsonEscape(v.relation) +
                "\",\"checkIn\":\"" + jsonEscape(v.checkIn) +
                "\",\"checkOut\":\"" + jsonEscape(v.checkOut) +
                "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get visitors error: " << e.what() << "\n";
        sendApiError(res, 500, "Could not retrieve visitor records.");
    }
});


// GET MY VISITORS - STUDENT ONLY
server.Get("/api/student/visitors",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Student"}))
        return;

    try
    {
        Session currentSession;

        if (!isValidSession(req, currentSession))
        {
            sendApiError(res, 401, "Please log in again.");
            return;
        }

        auto con = connectDatabase();

        std::vector<WebVisitor> visitors =
            getWebVisitors(con.get(), currentSession.userId);

        std::string json = "{\"success\":true,\"visitors\":[";

        for (size_t i = 0; i < visitors.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& v = visitors[i];

            json +=
                "{\"visitorId\":" + std::to_string(v.visitorId) +
                ",\"visitorName\":\"" + jsonEscape(v.visitorName) +
                "\",\"relation\":\"" + jsonEscape(v.relation) +
                "\",\"checkIn\":\"" + jsonEscape(v.checkIn) +
                "\",\"checkOut\":\"" + jsonEscape(v.checkOut) +
                "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get student visitors error: "
                  << e.what() << "\n";

        sendApiError(res, 500, "Could not retrieve your visitor records.");
    }
});



// 2. REGISTER VISITOR - WARDEN ONLY
server.Post("/api/visitors/register",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    if (!req.has_param("studentId") ||
        !req.has_param("visitorName") ||
        !req.has_param("relation"))
    {
        sendApiError(res, 400, "Please provide all visitor details.");
        return;
    }

    try
    {
        int studentId = std::stoi(req.get_param_value("studentId"));

        std::string visitorName = req.get_param_value("visitorName");
        std::string relation = req.get_param_value("relation");

        if (studentId <= 0 ||
            visitorName.empty() ||
            relation.empty())
        {
            sendApiError(res, 400, "Invalid visitor details.");
            return;
        }

        auto con = connectDatabase();

        bool success = addWebVisitor(
            con.get(),
            studentId,
            visitorName,
            relation
        );

        if (!success)
        {
            sendApiError(res, 400,
                "Visitor could not be registered. Check the Student ID.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Visitor registered successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Register visitor error: " << e.what() << "\n";
        sendApiError(res, 400, "Invalid visitor details or database error.");
    }
});


// 3. CHECK OUT VISITOR - WARDEN ONLY
server.Post("/api/visitors/checkout",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    if (!req.has_param("visitorId"))
    {
        sendApiError(res, 400, "Visitor ID is required.");
        return;
    }

    try
    {
        int visitorId = std::stoi(req.get_param_value("visitorId"));

        auto con = connectDatabase();

        bool success = checkoutWebVisitor(con.get(), visitorId);

        if (!success)
        {
            sendApiError(res, 404,
                "Visitor not found or already checked out.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Visitor checked out successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Visitor checkout error: " << e.what() << "\n";
        sendApiError(res, 400, "Invalid Visitor ID.");
    }
});


// 4. DELETE VISITOR RECORD - WARDEN ONLY
server.Post("/api/visitors/delete",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    if (!req.has_param("visitorId"))
    {
        sendApiError(res, 400, "Visitor ID is required.");
        return;
    }

    try
    {
        int visitorId = std::stoi(req.get_param_value("visitorId"));

        auto con = connectDatabase();

        bool success = deleteWebVisitor(con.get(), visitorId);

        if (!success)
        {
            sendApiError(res, 404, "Visitor record not found.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Visitor record deleted successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Delete visitor error: " << e.what() << "\n";
        sendApiError(res, 400, "Invalid Visitor ID.");
    }
});


// ================= COMPLAINT MANAGEMENT APIs =================


// 1. GET ALL COMPLAINTS - WARDEN ONLY
server.Get("/api/complaints",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    try
    {
        auto con = connectDatabase();

        std::vector<WebComplaint> complaints =
            getWebComplaints(con.get());

        std::string json = "{\"success\":true,\"complaints\":[";

        for (size_t i = 0; i < complaints.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& c = complaints[i];

            json +=
                "{\"complaintId\":" + std::to_string(c.complaintId) +
                ",\"studentId\":" + std::to_string(c.studentId) +
                ",\"studentName\":\"" + jsonEscape(c.studentName) +
                "\",\"studentEmail\":\"" + jsonEscape(c.studentEmail) +
                "\",\"category\":\"" + jsonEscape(c.category) +
                "\",\"description\":\"" + jsonEscape(c.description) +
                "\",\"status\":\"" + jsonEscape(c.status) +
                "\",\"assignedTo\":" + std::to_string(c.assignedTo) +
                ",\"assignedName\":\"" + jsonEscape(c.assignedName) +
                "\",\"createdAt\":\"" + jsonEscape(c.createdAt) +
                "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get complaints error: " << e.what() << "\n";
        sendApiError(res, 500, "Could not retrieve complaints.");
    }
});


// 2. SUBMIT COMPLAINT - STUDENT ONLY
server.Post("/api/complaints",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Student")
    {
        sendApiError(res, 403, "Only students can submit complaints.");
        return;
    }

    if (!req.has_param("category") ||
        !req.has_param("description"))
    {
        sendApiError(res, 400, "Please provide complaint details.");
        return;
    }

    try
    {
        std::string category = req.get_param_value("category");
        std::string description = req.get_param_value("description");

        if (category.empty() || category.length() > 50 ||
            description.empty() || description.length() > 5000)
        {
            sendApiError(res, 400, "Invalid complaint details.");
            return;
        }

        auto con = connectDatabase();

        bool success = addWebComplaint(
            con.get(),
            currentSession.userId,
            category,
            description
        );

        if (!success)
        {
            sendApiError(res, 400, "Could not submit complaint.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Complaint submitted successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Submit complaint error: " << e.what() << "\n";
        sendApiError(res, 400, "Could not submit complaint.");
    }
});


// 3. GET STUDENT'S OWN COMPLAINTS - STUDENT ONLY
server.Get("/api/student/complaints",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Student")
    {
        sendApiError(res, 403, "Access denied.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        std::vector<WebComplaint> complaints =
            getWebComplaints(con.get(), currentSession.userId);

        std::string json = "{\"success\":true,\"complaints\":[";

        for (size_t i = 0; i < complaints.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& c = complaints[i];

            json +=
                "{\"complaintId\":" + std::to_string(c.complaintId) +
                ",\"category\":\"" + jsonEscape(c.category) +
                "\",\"description\":\"" + jsonEscape(c.description) +
                "\",\"status\":\"" + jsonEscape(c.status) +
                "\",\"assignedName\":\"" + jsonEscape(c.assignedName) +
                "\",\"createdAt\":\"" + jsonEscape(c.createdAt) +
                "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get student complaints error: " << e.what() << "\n";
        sendApiError(res, 500, "Could not retrieve your complaints.");
    }
});


// 4. ASSIGN COMPLAINT TO MAINTENANCE STAFF - WARDEN ONLY
server.Post("/api/complaints/assign",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    if (!req.has_param("complaintId") ||
        !req.has_param("staffId"))
    {
        sendApiError(res, 400, "Complaint ID and Staff ID are required.");
        return;
    }

    try
    {
        int complaintId = std::stoi(req.get_param_value("complaintId"));
        int staffId = std::stoi(req.get_param_value("staffId"));

        if (complaintId <= 0 || staffId <= 0)
        {
            sendApiError(res, 400, "Invalid complaint or staff ID.");
            return;
        }

        auto con = connectDatabase();

        bool success = assignWebComplaint(
            con.get(),
            complaintId,
            staffId
        );

        if (!success)
        {
            sendApiError(res, 400,
                "Assignment failed. Check the complaint and Maintenance Staff ID.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Complaint assigned successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Assign complaint error: " << e.what() << "\n";
        sendApiError(res, 400, "Invalid assignment details.");
    }
});


// 5. GET ASSIGNED COMPLAINTS - MAINTENANCE ONLY
server.Get("/api/maintenance/complaints",
    [](const httplib::Request& req, httplib::Response& res)
{
    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (currentSession.role != "Maintenance")
    {
        sendApiError(res, 403, "Access denied.");
        return;
    }

    try
    {
        auto con = connectDatabase();

        std::vector<WebComplaint> complaints =
            getWebComplaints(con.get(), 0, currentSession.userId);

        std::string json = "{\"success\":true,\"complaints\":[";

        for (size_t i = 0; i < complaints.size(); i++)
        {
            if (i > 0)
                json += ",";

            const auto& c = complaints[i];

            json +=
                "{\"complaintId\":" + std::to_string(c.complaintId) +
                ",\"studentName\":\"" + jsonEscape(c.studentName) +
                "\",\"category\":\"" + jsonEscape(c.category) +
                "\",\"description\":\"" + jsonEscape(c.description) +
                "\",\"status\":\"" + jsonEscape(c.status) +
                "\",\"createdAt\":\"" + jsonEscape(c.createdAt) +
                "\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get maintenance complaints error: "
                  << e.what() << "\n";

        sendApiError(res, 500, "Could not retrieve assigned complaints.");
    }
});


// 6. UPDATE COMPLAINT STATUS - WARDEN OR ASSIGNED MAINTENANCE STAFF
server.Post("/api/complaints/status",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden", "Maintenance"}))
        return;

    Session currentSession;

    if (!isValidSession(req, currentSession))
    {
        sendApiError(res, 401, "Please log in again.");
        return;
    }

    if (!req.has_param("complaintId") ||
        !req.has_param("status"))
    {
        sendApiError(res, 400, "Complaint ID and status are required.");
        return;
    }

    try
    {
        int complaintId = std::stoi(req.get_param_value("complaintId"));
        std::string status = req.get_param_value("status");

        if (complaintId <= 0)
        {
            sendApiError(res, 400, "Invalid complaint ID.");
            return;
        }

        if (status != "Pending" &&
            status != "In Progress" &&
            status != "Resolved")
        {
            sendApiError(res, 400, "Invalid complaint status.");
            return;
        }

        auto con = connectDatabase();

        int maintenanceId = 0;

        if (currentSession.role == "Maintenance")
            maintenanceId = currentSession.userId;

        bool success = updateWebComplaintStatus(
            con.get(),
            complaintId,
            status,
            maintenanceId
        );

        if (!success)
        {
            sendApiError(res, 404,
                "Complaint not found or you are not assigned to it.");
            return;
        }

        res.set_content(
            "{\"success\":true,\"message\":\"Complaint status updated successfully.\"}",
            "application/json"
        );
    }
    catch (const std::exception& e)
    {
        std::cerr << "Update complaint status error: " << e.what() << "\n";
        sendApiError(res, 400, "Could not update complaint status.");
    }
});


// GET MAINTENANCE STAFF - WARDEN ONLY
server.Get("/api/warden/maintenance-staff",
    [](const httplib::Request& req, httplib::Response& res)
{
    if (!authorizeApiRole(req, res, {"Warden"}))
        return;

    try
    {
        auto con = connectDatabase();

        std::unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT user_id, name "
                "FROM users "
                "WHERE role = 'Maintenance' "
                "ORDER BY name"
            )
        );

        std::unique_ptr<sql::ResultSet> result(
            stmt->executeQuery()
        );

        std::string json = "{\"success\":true,\"users\":[";

        bool first = true;

        while (result->next())
        {
            if (!first)
                json += ",";

            first = false;

            json +=
                "{\"userId\":" + std::to_string(result->getInt("user_id")) +
                ",\"name\":\"" + jsonEscape(result->getString("name")) +
                "\",\"role\":\"Maintenance\"}";
        }

        json += "]}";

        res.set_header("Cache-Control", "no-store");
        res.set_content(json, "application/json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Get maintenance staff error: "
                  << e.what() << "\n";

        sendApiError(res, 500, "Could not retrieve Maintenance Staff.");
    }
});



    // -------------------------------------
    // ADMIN FEE MANAGEMENT API
    // -------------------------------------

    // GET ALL FEES OR SEARCH BY STUDENT ID
    server.Get("/api/admin/fees",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"}))
            return;

        try
        {
            int userId = 0;

            if (req.has_param("studentId"))
            {
                std::string studentId =
                    req.get_param_value("studentId");

                if (studentId.empty())
                {
                    sendApiError(res, 400, "Student ID is required.");
                    return;
                }

                userId = std::stoi(studentId);

                if (userId <= 0)
                {
                    sendApiError(res, 400, "Invalid student ID.");
                    return;
                }
            }

            auto con = connectDatabase();

            std::vector<WebFee> fees =
                getWebFees(con.get(), userId);

            std::string json =
                "{\"success\":true,\"fees\":[";

            for (size_t i = 0; i < fees.size(); i++)
            {
                if (i > 0)
                    json += ",";

                const auto& fee = fees[i];

                json +=
                    "{\"feeId\":" + std::to_string(fee.feeId) +
                    ",\"userId\":" + std::to_string(fee.userId) +
                    ",\"studentName\":\"" + jsonEscape(fee.studentName) +
                    "\",\"studentEmail\":\"" + jsonEscape(fee.studentEmail) +
                    "\",\"amount\":" + std::to_string(fee.amount) +
                    ",\"dueDate\":\"" + jsonEscape(fee.dueDate) +
                    "\",\"paymentDate\":\"" + jsonEscape(fee.paymentDate) +
                    "\",\"status\":\"" + jsonEscape(fee.status) +
                    "\"}";
            }

            json += "]}";

            res.set_header("Cache-Control", "no-store");
            res.set_content(json, "application/json");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get fees error: " << e.what() << "\n";
            sendApiError(res, 500, "Could not retrieve fee records.");
        }
    });


    // ADD A NEW FEE RECORD
    server.Post("/api/admin/fees",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"}))
            return;

        try
        {
            std::string studentId =
                req.get_param_value("studentId");

            std::string amountText =
                req.get_param_value("amount");

            std::string dueDate =
                req.get_param_value("dueDate");

            if (studentId.empty() ||
                amountText.empty() ||
                dueDate.empty())
            {
                sendApiError(res, 400, "All fields are required.");
                return;
            }

            size_t idPosition = 0;
            int userId = std::stoi(studentId, &idPosition);

            if (idPosition != studentId.size() || userId <= 0)
            {
                sendApiError(res, 400, "Invalid student ID.");
                return;
            }

            size_t amountPosition = 0;
            double amount = std::stod(amountText, &amountPosition);

            if (amountPosition != amountText.size() ||
                amount <= 0)
            {
                sendApiError(res, 400, "Invalid fee amount.");
                return;
            }

            auto con = connectDatabase();

            bool success = addWebFee(
                con.get(),
                userId,
                amount,
                dueDate
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Fee record added successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(
                    res,
                    400,
                    "Fee could not be added. Check the student ID and amount."
                );
            }
        }
        catch (const std::invalid_argument& e)
        {
            sendApiError(res, 400, "Invalid fee details.");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Add fee error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error while adding fee.");
        }
    });


    // -------------------------------------
    // ROOM MANAGEMENT API
    // -------------------------------------

    // GET ALL ROOMS
    // Admin, Warden and Student can view rooms.
    server.Get("/api/rooms",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin", "Warden", "Student"}))
            return;

        try
        {
            auto con = connectDatabase();
            std::vector<WebRoom> rooms = getAllWebRooms(con.get());

            std::string json = "{\"success\":true,\"rooms\":[";

            for (size_t i = 0; i < rooms.size(); i++)
            {
                if (i > 0) json += ",";

                const auto& room = rooms[i];

                json +=
                    "{\"roomId\":" + std::to_string(room.roomId) +
                    ",\"roomNumber\":\"" + jsonEscape(room.roomNumber) +
                    "\",\"hostelBlock\":\"" + jsonEscape(room.hostelBlock) +
                    "\",\"capacity\":" + std::to_string(room.capacity) +
                    ",\"currentOccupancy\":" + std::to_string(room.currentOccupancy) +
                    ",\"availableBeds\":" +
                        std::to_string(room.capacity - room.currentOccupancy) +
                    ",\"status\":\"" + jsonEscape(room.status) + "\"}";
            }

            json += "]}";

            res.set_header("Cache-Control", "no-store");
            res.set_content(json, "application/json");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get rooms error: " << e.what() << "\n";
            sendApiError(res, 500, "Could not retrieve rooms.");
        }
    });

    // ADD ROOM - ADMIN ONLY
    server.Post("/api/admin/rooms",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            auto con = connectDatabase();

            bool success = createWebRoom(
                con.get(),
                req.get_param_value("roomNumber"),
                req.get_param_value("hostelBlock"),
                std::stoi(req.get_param_value("capacity"))
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Room added successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 400, "Room could not be added.");
            }
        }
        catch (const std::invalid_argument& e)
        {
            sendApiError(res, 400, e.what());
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Add room SQL error: " << e.what() << "\n";
            sendApiError(res, 409, "Could not add room. Check whether the room number already exists.");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Add room error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error while adding room.");
        }
    });

    // UPDATE ROOM - ADMIN ONLY
    server.Put(R"(/api/admin/rooms/(\d+))",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            int roomId = std::stoi(req.matches[1].str());
            auto con = connectDatabase();

            bool success = updateWebRoom(
                con.get(),
                roomId,
                req.get_param_value("roomNumber"),
                req.get_param_value("hostelBlock"),
                std::stoi(req.get_param_value("capacity")),
                req.get_param_value("status")
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Room updated successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 404, "Room not found.");
            }
        }
        catch (const std::invalid_argument& e)
        {
            sendApiError(res, 400, e.what());
        }
        catch (const sql::SQLException& e)
        {
            std::cerr << "Update room SQL error: " << e.what() << "\n";
            sendApiError(res, 409, "Could not update room. Check whether the room number already exists.");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Update room error: " << e.what() << "\n";
            sendApiError(res, 500, "Server error while updating room.");
        }
    });

    // DELETE ROOM - ADMIN ONLY
    server.Delete(R"(/api/admin/rooms/(\d+))",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"})) return;

        try
        {
            int roomId = std::stoi(req.matches[1].str());
            auto con = connectDatabase();

            if (deleteWebRoom(con.get(), roomId))
            {
                res.set_content(
                    R"({"success":true,"message":"Room deleted successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 404, "Room not found.");
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Delete room error: " << e.what() << "\n";
            sendApiError(res, 400, e.what());
        }
    });


    // -------------------------------------
    // ROOM ALLOCATION API
    // -------------------------------------

    // GET ALLOCATION REQUESTS
    // Admin sees all requests; Student sees only their own.
    server.Get("/api/allocation/requests",
        [](const httplib::Request& req, httplib::Response& res)
    {
        Session currentSession;

        if (!isValidSession(req, currentSession))
        {
            sendApiError(res, 401, "Please log in again.");
            return;
        }

        if (currentSession.role != "Admin" &&
            currentSession.role != "Student")
        {
            sendApiError(res, 403, "Access denied.");
            return;
        }

        try
        {
            auto con = connectDatabase();

            bool adminView = currentSession.role == "Admin";

            std::vector<WebAllocationRequest> requests =
                getAllocationRequests(
                    con.get(),
                    currentSession.userId,
                    adminView
                );

            std::string json =
                "{\"success\":true,\"requests\":[";

            for (size_t i = 0; i < requests.size(); i++)
            {
                if (i > 0) json += ",";

                const auto& item = requests[i];

                json +=
                    "{\"requestId\":" + std::to_string(item.requestId) +
                    ",\"userId\":" + std::to_string(item.userId) +
                    ",\"studentName\":\"" + jsonEscape(item.studentName) +
                    "\",\"roomId\":" + std::to_string(item.roomId) +
                    ",\"roomNumber\":\"" + jsonEscape(item.roomNumber) +
                    "\",\"hostelBlock\":\"" + jsonEscape(item.hostelBlock) +
                    "\",\"feeId\":" + std::to_string(item.feeId) +
                    ",\"feeAmount\":" + std::to_string(item.feeAmount) +
                    ",\"requestDate\":\"" + jsonEscape(item.requestDate) +
                    "\",\"status\":\"" + jsonEscape(item.status) +
                    "\",\"feeStatus\":\"" + jsonEscape(item.feeStatus) +
                    "\"}";
            }

            json += "]}";

            res.set_header("Cache-Control", "no-store");
            res.set_content(json, "application/json");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get allocation requests error: "
                      << e.what() << "\n";

            sendApiError(res, 500, "Could not retrieve allocation requests.");
        }
    });


    // STUDENT SUBMITS ALLOCATION REQUEST
    server.Post("/api/allocation/request",
        [](const httplib::Request& req, httplib::Response& res)
    {
        Session currentSession;

        if (!isValidSession(req, currentSession))
        {
            sendApiError(res, 401, "Please log in again.");
            return;
        }

        if (currentSession.role != "Student")
        {
            sendApiError(res, 403, "Only students can submit requests.");
            return;
        }

        try
        {
            int roomId = std::stoi(req.get_param_value("roomId"));

            auto con = connectDatabase();

            bool success = submitAllocationRequest(
                con.get(),
                currentSession.userId,
                roomId
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Room request submitted successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(
                    res,
                    400,
                    "Request could not be submitted. Check room availability or existing requests."
                );
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Submit allocation request error: "
                      << e.what() << "\n";

            sendApiError(res, 400, "Invalid request or server error.");
        }
    });


    // ADMIN APPROVES REQUEST
    server.Post("/api/allocation/approve",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"}))
            return;

        try
        {
            int requestId = std::stoi(
                req.get_param_value("requestId")
            );

            auto con = connectDatabase();

            bool success = approveAllocationRequest(
                con.get(),
                requestId
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Request approved and fee generated."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(
                    res,
                    400,
                    "Request could not be approved. Check its status and room availability."
                );
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Approve request error: "
                      << e.what() << "\n";

            sendApiError(res, 400, "Could not approve request.");
        }
    });


    // ADMIN REJECTS REQUEST
    server.Post("/api/allocation/reject",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin"}))
            return;

        try
        {
            int requestId = std::stoi(
                req.get_param_value("requestId")
            );

            auto con = connectDatabase();

            bool success = rejectAllocationRequest(
                con.get(),
                requestId
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Request rejected."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 400, "Request could not be rejected.");
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Reject request error: "
                      << e.what() << "\n";

            sendApiError(res, 400, "Could not reject request.");
        }
    });


    // STUDENT PAYS APPROVED ALLOCATION FEE
    server.Post("/api/allocation/pay",
        [](const httplib::Request& req, httplib::Response& res)
    {
        Session currentSession;

        if (!isValidSession(req, currentSession))
        {
            sendApiError(res, 401, "Please log in again.");
            return;
        }

        if (currentSession.role != "Student")
        {
            sendApiError(res, 403, "Only students can pay allocation fees.");
            return;
        }

        try
        {
            int requestId = std::stoi(
                req.get_param_value("requestId")
            );

            auto con = connectDatabase();

            bool success = payAllocationFee(
                con.get(),
                requestId,
                currentSession.userId
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Payment successful. Room allocated."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(
                    res,
                    400,
                    "Payment or allocation failed. Check request, fee and room status."
                );
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Allocation payment error: "
                      << e.what() << "\n";

            sendApiError(res, 400, "Could not complete payment.");
        }
    });


    // GET CONFIRMED ALLOCATIONS
    // Admin and Warden can view all allocation records.
    server.Get("/api/allocations",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin", "Warden"}))
            return;

        try
        {
            auto con = connectDatabase();

            std::vector<WebAllocation> allocations =
                getWebAllocations(con.get());

            std::string json =
                "{\"success\":true,\"allocations\":[";

            for (size_t i = 0; i < allocations.size(); i++)
            {
                if (i > 0) json += ",";

                const auto& item = allocations[i];

                json +=
                    "{\"allocationId\":" + std::to_string(item.allocationId) +
                    ",\"userId\":" + std::to_string(item.userId) +
                    ",\"studentName\":\"" + jsonEscape(item.studentName) +
                    "\",\"roomId\":" + std::to_string(item.roomId) +
                    ",\"roomNumber\":\"" + jsonEscape(item.roomNumber) +
                    "\",\"allocationDate\":\"" + jsonEscape(item.allocationDate) +
                    "\",\"status\":\"" + jsonEscape(item.status) +
                    "\"}";
            }

            json += "]}";

            res.set_header("Cache-Control", "no-store");
            res.set_content(json, "application/json");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Get allocations error: "
                      << e.what() << "\n";

            sendApiError(res, 500, "Could not retrieve allocations.");
        }
    });


    // ADMIN OR WARDEN VACATES AN ALLOCATION
    server.Post("/api/allocation/vacate",
        [](const httplib::Request& req, httplib::Response& res)
    {
        if (!authorizeApiRole(req, res, {"Admin", "Warden"}))
            return;

        try
        {
            int allocationId = std::stoi(
                req.get_param_value("allocationId")
            );

            auto con = connectDatabase();

            bool success = vacateWebAllocation(
                con.get(),
                allocationId
            );

            if (success)
            {
                res.set_content(
                    R"({"success":true,"message":"Room vacated successfully."})",
                    "application/json"
                );
            }
            else
            {
                sendApiError(res, 400, "Allocation could not be vacated.");
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Vacate allocation error: "
                      << e.what() << "\n";

            sendApiError(res, 400, "Could not vacate allocation.");
        }
    });


    // -------------------------------------
    // LOGOUT
    // -------------------------------------

    auto logoutHandler = [](const httplib::Request& req, httplib::Response& res)
    {
        std::string token = getSessionToken(req);

        if (!token.empty())
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions.erase(token);
        }

        res.set_header(
            "Set-Cookie",
            "SHMS_SESSION=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0"
        );

        res.set_header("Cache-Control", "no-store");

        res.set_content(
            R"({"success":true,"message":"Logged out successfully."})",
            "application/json"
        );
    };

    server.Post("/api/logout", logoutHandler);

    // Supports existing dashboard links that use GET.
    server.Get("/api/logout", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string token = getSessionToken(req);

        if (!token.empty())
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions.erase(token);
        }

        res.set_header(
            "Set-Cookie",
            "SHMS_SESSION=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0"
        );

        res.status = 302;
        res.set_header("Location", "/login.html");
    });

    // -------------------------------------
    // FRONTEND CSS AND JAVASCRIPT
    // -------------------------------------

    if (!server.set_mount_point("/css", "./frontend/css"))
    {
        std::cerr << "Error: CSS folder not found!\n";
        return 1;
    }

    if (!server.set_mount_point("/js", "./frontend/js"))
    {
        std::cerr << "Error: JavaScript folder not found!\n";
        return 1;
    }

    // -------------------------------------
    // START SERVER
    // -------------------------------------

    std::cout << "Smart Hostel Web Server Started!\n";
    std::cout << "Open: http://127.0.0.1:8080/login.html\n";
    std::cout << "Press Ctrl+C to stop the server.\n";

    if (!server.listen("127.0.0.1", 8080))
    {
        std::cerr << "Could not start server on port 8080.\n";
        return 1;
    }

    return 0;
}
