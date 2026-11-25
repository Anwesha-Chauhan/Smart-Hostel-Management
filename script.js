
console.log("Smart Hostel Management UI loaded successfully!");


// ================= USER MANAGEMENT =================

let editingUserId = null;

// Safely display user input
function escapeHTML(value) {
    const element = document.createElement("span");
    element.textContent = value ?? "";
    return element.innerHTML;
}

// API response handler
async function handleApiResponse(response) {
    const data = await response.json().catch(() => ({}));

    if (!response.ok || !data.success) {
        throw new Error(data.message || "Something went wrong.");
    }

    return data;
}

// Load users from MySQL
async function loadUsers() {

    const table = document.getElementById("studentTable");

    if (!table) return;

    table.innerHTML = `
        <tr>
            <td colspan="6">Loading users...</td>
        </tr>
    `;

    try {

        const response = await fetch("/api/admin/users");

        const data = await handleApiResponse(response);

        displayUsers(data.users);

    }
    catch (error) {

        console.error("Load users error:", error);

        table.innerHTML = `
            <tr>
                <td colspan="6">Could not load users. ${escapeHTML(error.message)}</td>
            </tr>
        `;
    }
}

// Display users in the table
function displayUsers(users) {

    const table = document.getElementById("studentTable");

    if (!table) return;

    table.innerHTML = "";

    if (!users || users.length === 0) {

        table.innerHTML = `
            <tr>
                <td colspan="6">No users found.</td>
            </tr>
        `;

        return;
    }

    users.forEach(user => {

        const row = document.createElement("tr");

        row.dataset.userId = user.userId;

        const values = [
            user.userId,
            user.name,
            user.email,
            user.phone,
            user.role
        ];

        values.forEach(value => {

            const cell = document.createElement("td");

            cell.textContent = value ?? "";

            row.appendChild(cell);
        });

        const actionCell = document.createElement("td");

        actionCell.innerHTML = `
            <button class="edit-btn">Edit</button>
            <button class="delete-btn">Delete</button>
        `;

        row.appendChild(actionCell);

        table.appendChild(row);

    });
}

// Open Add User form
function openStudentForm() {

    editingUserId = null;

    const form = document.getElementById("studentForm");

    if (!form) return;

    form.reset();

    document.querySelector("#studentModal .modal-content h2")
        .textContent = "Add New User";

    document.querySelector("#studentForm button[type='submit']")
        .textContent = "Save User";

    document.getElementById("userPassword").required = true;

    document.getElementById("userPassword").placeholder =
        "Password (minimum 8 characters)";

    document.getElementById("studentModal").style.display = "flex";
}

// Close form
function closeStudentForm() {

    document.getElementById("studentModal").style.display = "none";

    document.getElementById("studentForm").reset();

    editingUserId = null;
}

// Search users
document.getElementById("searchStudent")?.addEventListener(
    "input",
    function() {

        const searchValue = this.value.toLowerCase();

        document.querySelectorAll("#studentTable tr").forEach(row => {

            row.style.display =
                row.innerText.toLowerCase().includes(searchValue)
                    ? ""
                    : "none";

        });

    }
);

// Add or Update User
document.getElementById("studentForm")?.addEventListener(
    "submit",
    async function(event) {

        event.preventDefault();

        const name = document.getElementById("studentName").value.trim();

        const email = document.getElementById("studentEmail").value.trim();

        const phone = document.getElementById("studentPhone").value.trim();

        const role = document.getElementById("userRole").value;

        const password = document.getElementById("userPassword").value;

        if (!name || !email || !phone || !role) {

            alert("Please fill all user details.");

            return;
        }

        if (!editingUserId && password.length < 8) {

            alert("Password must contain at least 8 characters.");

            return;
        }

        if (editingUserId && password && password.length < 8) {

            alert("New password must contain at least 8 characters.");

            return;
        }

        const formData = new URLSearchParams();

        formData.append("name", name);
        formData.append("email", email);
        formData.append("phone", phone);
        formData.append("role", role);
        formData.append("password", password);

        const submitButton =
            document.querySelector("#studentForm button[type='submit']");

        submitButton.disabled = true;

        try {

            let response;

            if (editingUserId) {

                response = await fetch(
                    `/api/admin/users/${editingUserId}`,
                    {
                        method: "PUT",
                        headers: {
                            "Content-Type": "application/x-www-form-urlencoded"
                        },
                        body: formData
                    }
                );

            } else {

                response = await fetch("/api/admin/users", {
                    method: "POST",
                    headers: {
                        "Content-Type": "application/x-www-form-urlencoded"
                    },
                    body: formData
                });

            }

            const data = await handleApiResponse(response);

            alert(data.message);

            closeStudentForm();

            await loadUsers();

        }
        catch (error) {

            console.error("Save user error:", error);

            alert(error.message);

        }
        finally {

            submitButton.disabled = false;

        }

    }
);

// Handle Edit and Delete
document.getElementById("studentTable")?.addEventListener(
    "click",
    async function(event) {

        const row = event.target.closest("tr");

        if (!row || !row.dataset.userId) return;

        const userId = row.dataset.userId;

        // Edit user
        if (event.target.closest(".edit-btn")) {

            editingUserId = userId;

            document.getElementById("studentName").value =
                row.cells[1].textContent;

            document.getElementById("studentEmail").value =
                row.cells[2].textContent;

            document.getElementById("studentPhone").value =
                row.cells[3].textContent;

            document.getElementById("userRole").value =
                row.cells[4].textContent;

            document.getElementById("userPassword").value = "";

            document.getElementById("userPassword").required = false;

            document.getElementById("userPassword").placeholder =
                "Leave blank to keep current password";

            document.querySelector("#studentModal .modal-content h2")
                .textContent = "Edit User";

            document.querySelector("#studentForm button[type='submit']")
                .textContent = "Update User";

            document.getElementById("studentModal").style.display = "flex";

        }

        // Delete user
        if (event.target.closest(".delete-btn")) {

            const userName = row.cells[1].textContent;

            if (!confirm(`Are you sure you want to delete ${userName}?`)) {
                return;
            }

            try {

                const response = await fetch(
                    `/api/admin/users/${userId}`,
                    {
                        method: "DELETE"
                    }
                );

                const data = await handleApiResponse(response);

                alert(data.message);

                await loadUsers();

            }
            catch (error) {

                console.error("Delete user error:", error);

                alert(error.message);

            }

        }

    }
);

// Load real users when the page opens
loadUsers();



// ================= ROOM MANAGEMENT =================

let roomView = "all";
let editingRoomId = null;
let roomData = [];
let currentUserRole = "";

// Monthly price according to sharing type
const roomPricing = {
    1: 12000,
    2: 8000,
    3: 6000
};

// Get sharing type
function getSharingType(capacity) {
    const sharingTypes = {
        1: "Single Room",
        2: "Double Sharing",
        3: "Triple Sharing"
    };

    return sharingTypes[capacity] || "Unknown";
}

// Get monthly room price
function getRoomPrice(capacity) {
    return roomPricing[capacity] || 0;
}

// Format price
function formatRoomPrice(price) {
    return "₹" + price.toLocaleString("en-IN");
}

// Escape HTML
function escapeRoomHTML(value) {
    return String(value ?? "").replace(/[&<>"']/g, char => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;"
    })[char]);
}

// Show API errors
async function getRoomApiError(response) {
    try {
        const result = await response.json();
        return result.message || "Something went wrong.";
    } catch {
        return "Server error. Please try again.";
    }
}

// Get current logged-in user's role
async function loadRoomUserRole() {
    try {
        const response = await fetch("/api/session");

        if (!response.ok) {
            window.location.href = "/login.html";
            return;
        }

        const result = await response.json();

        currentUserRole = result.role;

        const isAdmin = currentUserRole === "Admin";

        // Only Admin can manage rooms
        const addButton = document.getElementById("addRoomButton");
        const actionsHeading = document.getElementById("roomActionsHeading");
        const usersNav = document.getElementById("usersNavLink");
        const dashboardLink = document.getElementById("dashboardLink");

        if (addButton) {
            addButton.style.display = isAdmin ? "" : "none";
        }

        if (actionsHeading) {
            actionsHeading.style.display = isAdmin ? "" : "none";
        }

        if (usersNav && !isAdmin) {
            usersNav.style.display = "none";
        }

        if (dashboardLink) {
            const dashboards = {
                Admin: "/index.html",
                Warden: "/dashboards/warden.html",
                Student: "/dashboards/student.html"
            };

            dashboardLink.href = dashboards[currentUserRole] || "/login.html";
        }

        // Hide allocation navigation until its server page is integrated.
        const allocationNav = document.getElementById("allocationNavLink");

        if (allocationNav) {
            allocationNav.style.display = "none";
        }

    } catch (error) {
        console.error("Could not load user role:", error);
        alert("Could not verify your login. Please log in again.");
        window.location.href = "/login.html";
    }
}

// Fetch rooms from MySQL
async function loadRooms() {
    const tableBody = document.getElementById("roomTableBody");

    if (!tableBody) return;

    tableBody.innerHTML = `
        <tr>
            <td colspan="9">Loading rooms...</td>
        </tr>
    `;

    try {
        const response = await fetch("/api/rooms");

        if (!response.ok) {
            if (response.status === 401) {
                window.location.href = "/login.html";
                return;
            }

            throw new Error(await getRoomApiError(response));
        }

        const result = await response.json();

        roomData = result.rooms || [];

        displayRooms();

    } catch (error) {
        console.error("Room loading error:", error);

        tableBody.innerHTML = `
            <tr>
                <td colspan="9">
                    ${escapeRoomHTML(error.message)}
                </td>
            </tr>
        `;
    }
}

// Display room table
function displayRooms() {
    const tableBody = document.getElementById("roomTableBody");
    const searchInput = document.getElementById("roomSearch");
    const count = document.getElementById("roomCount");

    if (!tableBody) return;

    const search = searchInput
        ? searchInput.value.trim().toLowerCase()
        : "";

    const filteredRooms = roomData.filter(room => {

        const matchesSearch =
            room.roomNumber.toLowerCase().includes(search) ||
            room.hostelBlock.toLowerCase().includes(search) ||
            getSharingType(room.capacity).toLowerCase().includes(search);

        const matchesView =
            roomView === "all" ||
            (
                room.status === "Available" &&
                room.currentOccupancy < room.capacity
            );

        return matchesSearch && matchesView;
    });

    tableBody.innerHTML = "";

    if (filteredRooms.length === 0) {
        tableBody.innerHTML = `
            <tr>
                <td colspan="9">No rooms found.</td>
            </tr>
        `;
    }

    filteredRooms.forEach(room => {

        const row = document.createElement("tr");

        const monthlyPrice = formatRoomPrice(
            getRoomPrice(room.capacity)
        );

        const statusHTML = `
            <span class="status ${room.status.toLowerCase()}">
                ${escapeRoomHTML(room.status)}
            </span>
        `;

        const actionHTML = currentUserRole === "Admin"
            ? `
                <button class="edit-btn" data-id="${room.roomId}">
                    Edit
                </button>

                <button class="delete-btn" data-id="${room.roomId}">
                    Delete
                </button>
              `
            : "";

        row.innerHTML = `
            <td>${room.roomId}</td>
            <td>${escapeRoomHTML(room.roomNumber)}</td>
            <td>${escapeRoomHTML(room.hostelBlock)}</td>
            <td>${getSharingType(room.capacity)}</td>
            <td>${room.capacity}</td>
            <td>${room.currentOccupancy}</td>
            <td>${monthlyPrice}</td>
            <td>${statusHTML}</td>
            <td style="display:${currentUserRole === "Admin" ? "" : "none"}">
                ${actionHTML}
            </td>
        `;

        tableBody.appendChild(row);
    });

    if (count) {
        count.textContent =
            `Showing ${filteredRooms.length} ${
                roomView === "all" ? "rooms" : "available rooms"
            }`;
    }
}

// View all rooms
function showAllRooms() {
    roomView = "all";
    displayRooms();
}

// View available rooms
function showAvailableRooms() {
    roomView = "available";
    displayRooms();
}

// Open add/edit room form
function openRoomForm(room = null) {

    if (currentUserRole !== "Admin") {
        alert("Only Admin can manage rooms.");
        return;
    }

    const modal = document.getElementById("roomModal");
    const form = document.getElementById("roomForm");

    if (!modal || !form) return;

    form.reset();

    editingRoomId = room ? room.roomId : null;

    document.getElementById("roomId").value =
        room ? room.roomId : "";

    document.getElementById("roomNumber").value =
        room ? room.roomNumber : "";

    document.getElementById("roomBlock").value =
        room ? room.hostelBlock : "";

    document.getElementById("roomCapacity").value =
        room ? room.capacity : "2";

    document.getElementById("roomOccupancy").value =
        room ? room.currentOccupancy : 0;

    document.getElementById("roomStatus").value =
        room ? room.status : "Available";

    document.getElementById("roomModalTitle").textContent =
        room ? "Edit Room" : "Add Room";

    modal.style.display = "flex";
}

// Close room form
function closeRoomForm() {
    const modal = document.getElementById("roomModal");

    if (modal) {
        modal.style.display = "none";
    }
}

// Save room to MySQL
document.getElementById("roomForm")?.addEventListener(
    "submit",
    async function(e) {

        e.preventDefault();

        if (currentUserRole !== "Admin") {
            alert("Only Admin can manage rooms.");
            return;
        }

        const roomNumber =
            document.getElementById("roomNumber").value.trim();

        const block =
            document.getElementById("roomBlock").value.trim();

        const capacity =
            Number(document.getElementById("roomCapacity").value);

        const occupancy =
            Number(document.getElementById("roomOccupancy").value);

        const status =
            document.getElementById("roomStatus").value;

        if (
            !roomNumber ||
            !block ||
            ![1, 2, 3].includes(capacity) ||
            occupancy < 0 ||
            occupancy > capacity
        ) {
            alert("Please enter valid room details.");
            return;
        }

        if (occupancy === capacity && status === "Available") {
            alert("A full room cannot be marked Available.");
            return;
        }

        const submitButton =
            this.querySelector('button[type="submit"]');

        submitButton.disabled = true;

        try {

            const formData = new URLSearchParams();

            formData.append("roomNumber", roomNumber);
            formData.append("hostelBlock", block);
            formData.append("capacity", capacity);
            formData.append("status", status);

            let url = "/api/admin/rooms";
            let method = "POST";

            if (editingRoomId !== null) {
                url = `/api/admin/rooms/${editingRoomId}`;
                method = "PUT";
            }

            const response = await fetch(url, {
                method: method,
                headers: {
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                body: formData.toString()
            });

            if (!response.ok) {
                throw new Error(await getRoomApiError(response));
            }

            const result = await response.json();

            alert(result.message || "Room saved successfully.");

            closeRoomForm();

            await loadRooms();

        } catch (error) {
            console.error("Save room error:", error);
            alert(error.message);

        } finally {
            submitButton.disabled = false;
        }
    }
);

// Edit and Delete room
document.getElementById("roomTableBody")?.addEventListener(
    "click",
    async function(e) {

        if (currentUserRole !== "Admin") return;

        const editButton = e.target.closest(".edit-btn");
        const deleteButton = e.target.closest(".delete-btn");

        if (editButton) {

            const roomId = Number(editButton.dataset.id);

            const room = roomData.find(
                item => item.roomId === roomId
            );

            if (room) {
                openRoomForm(room);
            }
        }

        if (deleteButton) {

            const roomId = Number(deleteButton.dataset.id);

            const room = roomData.find(
                item => item.roomId === roomId
            );

            if (!room) return;

            if (!confirm(
                `Are you sure you want to delete room ${room.roomNumber}?`
            )) {
                return;
            }

            try {

                const response = await fetch(
                    `/api/admin/rooms/${roomId}`,
                    {
                        method: "DELETE"
                    }
                );

                if (!response.ok) {
                    throw new Error(await getRoomApiError(response));
                }

                const result = await response.json();

                alert(result.message || "Room deleted successfully.");

                await loadRooms();

            } catch (error) {
                console.error("Delete room error:", error);
                alert(error.message);
            }
        }
    }
);

// Room search
document.getElementById("roomSearch")?.addEventListener(
    "input",
    displayRooms
);

// Initialize room page
if (document.getElementById("roomTableBody")) {
    (async function initializeRoomPage() {
        await loadRoomUserRole();
        await loadRooms();
    })();
}


// ================= ROOM ALLOCATION =================

// This section uses the backend APIs and MySQL database.
// No demo room, student, or allocation data is used.

const allocationFormSection = document.querySelector(".allocation-form-section");
const allocationSummary = document.querySelector(".allocation-summary");
const allocationTableContainer = document.querySelector(".table-container");

let allocationUserRole = "";
let allocationRooms = [];
let allocationRequests = [];
let allocationRecords = [];

// Safely display text received from the backend.
function allocationEscapeHTML(value) {
    return String(value ?? "").replace(/[&<>"']/g, character => ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;"
    })[character]);
}

// Send form data to the backend.
async function allocationPost(url, data) {
    const response = await fetch(url, {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded"
        },
        body: new URLSearchParams(data)
    });

    const result = await response.json();

    if (!response.ok || !result.success) {
        throw new Error(result.message || "Request failed.");
    }

    return result;
}

// Load the currently logged-in user's role.
async function loadAllocationSession() {
    const response = await fetch("/api/session");
    const result = await response.json();

    if (!response.ok || !result.success) {
        window.location.href = "/login.html";
        return;
    }

    allocationUserRole = result.role;
}

// Load rooms from MySQL.
async function loadAllocationRooms() {
    const response = await fetch("/api/rooms");
    const result = await response.json();

    if (!response.ok || !result.success) {
        throw new Error(result.message || "Unable to load rooms.");
    }

    allocationRooms = result.rooms || [];
}

// Display available rooms in the Student dropdown.
function refreshAvailableRooms() {
    const roomSelect = document.getElementById("allocationRoom");

    if (!roomSelect) return;

    roomSelect.innerHTML = '<option value="">Choose a room</option>';

    allocationRooms.forEach(room => {
        if (
            room.status === "Available" &&
            Number(room.currentOccupancy) < Number(room.capacity)
        ) {
            const vacantBeds = Number(room.availableBeds);

            const option = document.createElement("option");
            option.value = room.roomId;
            option.textContent =
                `${room.roomNumber} - ${room.hostelBlock} (${vacantBeds} vacant bed${vacantBeds === 1 ? "" : "s"})`;

            roomSelect.appendChild(option);
        }
    });
}

// Load allocation requests from MySQL.
async function loadAllocationRequests() {
    const response = await fetch("/api/allocation/requests");
    const result = await response.json();

    if (!response.ok || !result.success) {
        throw new Error(result.message || "Unable to load allocation requests.");
    }

    allocationRequests = result.requests || [];
}

// Load confirmed allocations from MySQL.
async function loadConfirmedAllocations() {
    const response = await fetch("/api/allocations");
    const result = await response.json();

    if (!response.ok || !result.success) {
        throw new Error(result.message || "Unable to load allocations.");
    }

    allocationRecords = result.allocations || [];
}

// Render the Student request form.
function renderStudentAllocationForm() {
    allocationFormSection.innerHTML = `
        <h2>Request a Room</h2>

        <form id="allocationForm">
            <label for="allocationRoom">Select Available Room</label>

            <select id="allocationRoom" required>
                <option value="">Loading rooms...</option>
            </select>

            <button type="submit" class="add-btn">
                Submit Room Request
            </button>
        </form>

        <p>Your request will be sent to the Admin for approval.</p>
    `;

    refreshAvailableRooms();

    document.getElementById("allocationForm")
        .addEventListener("submit", async function(event) {
            event.preventDefault();

            const roomId = document.getElementById("allocationRoom").value;

            if (!roomId) {
                alert("Please select a room.");
                return;
            }

            try {
                await allocationPost("/api/allocation/request", {
                    roomId: roomId
                });

                alert("Room request submitted successfully!");

                await loadAllocationRequests();
                await loadAllocationRooms();

                displayStudentRequests();
                refreshAvailableRooms();

            } catch (error) {
                alert(error.message);
            }
        });
}

// Render Admin's request-management section.
function renderAdminAllocationSection() {
    allocationFormSection.innerHTML = `
        <h2>Room Allocation Requests</h2>
        <p>Review student requests and approve or reject them.</p>
    `;
}

// Render the allocation summary.
function updateAllocationSummary() {
    if (!allocationSummary) return;

    const total = allocationRecords.length;

    const active = allocationRecords.filter(
        record => record.status === "Active"
    ).length;

    const vacated = allocationRecords.filter(
        record => record.status === "Vacated"
    ).length;

    allocationSummary.innerHTML = `
        <div class="allocation-card">
            <h3>Total Allocations</h3>
            <p>${total}</p>
        </div>

        <div class="allocation-card">
            <h3>Active Allocations</h3>
            <p>${active}</p>
        </div>

        <div class="allocation-card">
            <h3>Vacated Rooms</h3>
            <p>${vacated}</p>
        </div>
    `;
}

// Display Student's own requests and fee status.
function displayStudentRequests() {
    if (!allocationTableContainer) return;

    allocationTableContainer.innerHTML = `
        <h2>My Room Requests</h2>

        <table>
            <thead>
                <tr>
                    <th>Request ID</th>
                    <th>Room</th>
                    <th>Block</th>
                    <th>Request Date</th>
                    <th>Request Status</th>
                    <th>Fee Amount</th>
                    <th>Fee Status</th>
                    <th>Action</th>
                </tr>
            </thead>

            <tbody id="allocationTableBody"></tbody>
        </table>
    `;

    const tbody = document.getElementById("allocationTableBody");

    if (allocationRequests.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="8">You have not submitted any room requests.</td>
            </tr>
        `;
        return;
    }

    allocationRequests.forEach(request => {
        const feeStatus = request.feeStatus || "Not Generated";

        let action = "—";

        if (
            request.status === "Approved" &&
            feeStatus === "Pending"
        ) {
            action = `
                <button class="pay-allocation-btn"
                        data-id="${request.requestId}">
                    Pay Fee
                </button>
            `;
        } else if (
            request.status === "Approved" &&
            feeStatus === "Paid"
        ) {
            action = "Allocation Confirmed";
        }

        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${request.requestId}</td>
            <td>${allocationEscapeHTML(request.roomNumber)}</td>
            <td>${allocationEscapeHTML(request.hostelBlock)}</td>
            <td>${allocationEscapeHTML(request.requestDate)}</td>
            <td>${allocationEscapeHTML(request.status)}</td>
            <td>${request.feeAmount > 0 ? "₹" + Number(request.feeAmount).toLocaleString("en-IN") : "—"}</td>
            <td>${allocationEscapeHTML(feeStatus)}</td>
            <td>${action}</td>
        `;

        tbody.appendChild(row);
    });
}

// Display requests for Admin.
function displayAdminRequests() {
    if (!allocationTableContainer) return;

    allocationTableContainer.innerHTML = `
        <h2>Pending and Processed Requests</h2>

        <table>
            <thead>
                <tr>
                    <th>Request ID</th>
                    <th>Student</th>
                    <th>Room</th>
                    <th>Block</th>
                    <th>Request Date</th>
                    <th>Status</th>
                    <th>Action</th>
                </tr>
            </thead>

            <tbody id="allocationTableBody"></tbody>
        </table>
    `;

    const tbody = document.getElementById("allocationTableBody");

    if (allocationRequests.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="7">No room requests found.</td>
            </tr>
        `;
        return;
    }

    allocationRequests.forEach(request => {
        let action = "—";

        if (request.status === "Pending") {
            action = `
                <button class="approve-allocation-btn"
                        data-id="${request.requestId}">
                    Approve
                </button>

                <button class="reject-allocation-btn"
                        data-id="${request.requestId}">
                    Reject
                </button>
            `;
        }

        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${request.requestId}</td>
            <td>${allocationEscapeHTML(request.studentName)}</td>
            <td>${allocationEscapeHTML(request.roomNumber)}</td>
            <td>${allocationEscapeHTML(request.hostelBlock)}</td>
            <td>${allocationEscapeHTML(request.requestDate)}</td>
            <td>${allocationEscapeHTML(request.status)}</td>
            <td>${action}</td>
        `;

        tbody.appendChild(row);
    });
}

// Display confirmed allocations for Admin and Warden.
function displayConfirmedAllocations() {
    if (!allocationTableContainer) return;

    allocationTableContainer.innerHTML = `
        <h2>Confirmed Allocation Records</h2>

        <table>
            <thead>
                <tr>
                    <th>Allocation ID</th>
                    <th>Student</th>
                    <th>Room</th>
                    <th>Allocation Date</th>
                    <th>Status</th>
                    <th>Action</th>
                </tr>
            </thead>

            <tbody id="allocationTableBody"></tbody>
        </table>
    `;

    const tbody = document.getElementById("allocationTableBody");

    if (allocationRecords.length === 0) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6">No confirmed allocations found.</td>
            </tr>
        `;
        updateAllocationSummary();
        return;
    }

    allocationRecords.forEach(record => {
        let action = "—";

        if (record.status === "Active") {
            action = `
                <button class="vacate-btn"
                        data-id="${record.allocationId}">
                    Vacate
                </button>
            `;
        }

        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${record.allocationId}</td>
            <td>${allocationEscapeHTML(record.studentName)}</td>
            <td>${allocationEscapeHTML(record.roomNumber)}</td>
            <td>${allocationEscapeHTML(record.allocationDate)}</td>
            <td>${allocationEscapeHTML(record.status)}</td>
            <td>${action}</td>
        `;

        tbody.appendChild(row);
    });

    updateAllocationSummary();
}

// Handle Admin approval and rejection, Student payment,
// and Admin/Warden room vacation.
if (allocationTableContainer) {
    allocationTableContainer.addEventListener("click", async function(event) {
        const button = event.target.closest("button");

        if (!button) return;

        const requestId = button.dataset.id;

        try {
            // Admin approves a request.
            if (button.classList.contains("approve-allocation-btn")) {
                if (!confirm("Approve this room request?")) return;

                await allocationPost("/api/allocation/approve", {
                    requestId: requestId
                });

                alert("Request approved. Fee generated successfully.");

                await loadAllocationRequests();
                displayAdminRequests();
            }

            // Admin rejects a request.
            else if (button.classList.contains("reject-allocation-btn")) {
                if (!confirm("Reject this room request?")) return;

                await allocationPost("/api/allocation/reject", {
                    requestId: requestId
                });

                alert("Room request rejected.");

                await loadAllocationRequests();
                displayAdminRequests();
            }

            // Student pays the approved fee.
            else if (button.classList.contains("pay-allocation-btn")) {
                if (!confirm("Proceed with simulated fee payment?")) return;

                await allocationPost("/api/allocation/pay", {
                    requestId: requestId
                });

                alert("Payment successful! Room allocation confirmed.");

                await loadAllocationRequests();
                await loadAllocationRooms();

                displayStudentRequests();
            }

            // Admin/Warden vacates an active allocation.
            else if (button.classList.contains("vacate-btn")) {
                const allocation = allocationRecords.find(
                    record => String(record.allocationId) === String(requestId)
                );

                if (!allocation) return;

                if (!confirm(`Vacate room ${allocation.roomNumber} for ${allocation.studentName}?`)) {
                    return;
                }

                await allocationPost("/api/allocation/vacate", {
                    allocationId: requestId
                });

                alert("Room vacated successfully.");

                await loadConfirmedAllocations();
                await loadAllocationRooms();

                displayConfirmedAllocations();
            }

        } catch (error) {
            alert(error.message);
        }
    });
}

// Initialize the allocation page according to the logged-in role.
async function initializeAllocationPage() {
    if (!allocationFormSection || !allocationTableContainer) return;

    try {
        await loadAllocationSession();
        await loadAllocationRooms();

        if (allocationUserRole === "Student") {
            allocationSummary?.classList.add("hidden");

            renderStudentAllocationForm();

            await loadAllocationRequests();
            displayStudentRequests();
        }

        else if (allocationUserRole === "Admin") {
            renderAdminAllocationSection();

            await loadAllocationRequests();
            displayAdminRequests();

            await loadConfirmedAllocations();
            updateAllocationSummary();
        }

        else if (allocationUserRole === "Warden") {
            allocationFormSection.innerHTML = `
                <h2>Room Allocation Records</h2>
                <p>View confirmed allocations and vacate rooms when required.</p>
            `;

            await loadConfirmedAllocations();
            displayConfirmedAllocations();
        }

        else {
            allocationFormSection.innerHTML =
                "<p>You do not have permission to access room allocation.</p>";

            allocationTableContainer.innerHTML = "";
        }

    } catch (error) {
        console.error("Allocation page error:", error);

        allocationFormSection.innerHTML = `
            <p>Unable to load allocation data: ${allocationEscapeHTML(error.message)}</p>
        `;
    }
}

// Run only on the allocation page.
initializeAllocationPage();
