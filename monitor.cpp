#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/inotify.h>
#include <limits.h>
#include <bits/basic_string.h>

namespace fs = std::filesystem;

// ============================================================
// Configuration
// ============================================================

constexpr std::string_view LOG_FILE = "/home/saif/Projects/Checkmate/file_access.log";
const fs::path COPY_ROOT = "/home/saif/Projects/Checkmate_Files";

// ============================================================
// C++20 Chrono + Format for Timestamp
// ============================================================
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================================
// Helper: Log event to file
// ============================================================
void log_event(std::string_view event_type, const fs::path& path) {
    std::ofstream log(std::string(LOG_FILE), std::ios::app);
    if (log.is_open()) {
        log << get_timestamp() << " | " << event_type << " | " << path.string() << '\n';
    }
}

// ============================================================
// Helper: Get human-readable event name
// ============================================================
constexpr std::string_view get_event_name(uint32_t mask) {
    if (mask & IN_ACCESS) return "ACCESSED";
    if (mask & IN_OPEN) return "OPENED";
    if (mask & IN_MODIFY) return "MODIFIED";
    if (mask & IN_CLOSE_WRITE) return "CLOSE_WRITE";
    if (mask & IN_CLOSE_NOWRITE) return "CLOSE_READ";
    if (mask & IN_CREATE) return "CREATED";
    if (mask & IN_DELETE) return "DELETED";
    if (mask & IN_MOVED_FROM) return "MOVED_FROM";
    if (mask & IN_MOVED_TO) return "MOVED_TO";
    return "UNKNOWN";
}

// ============================================================
// Helper: Copy file to backup directory
// ============================================================
void backup_file(const fs::path& src_path, const fs::path& dest_root, const fs::path& watch_root) {
    std::error_code ec;
    
    if (!fs::is_regular_file(src_path, ec) || ec) return;

    fs::path rel_path = fs::relative(src_path, watch_root, ec);
    if (ec || rel_path.empty()) return;
    
    // Check for path traversal attempts
    if (rel_path.string().compare(0, 2, "..") == 0) return;

    fs::path dest_path = dest_root / rel_path;

    fs::create_directories(dest_path.parent_path(), ec);
    if (ec) {
        std::cerr << "Failed to create directory for " << dest_path.string() << ": " << ec.message() << '\n';
        return;
    }

    fs::copy_file(src_path, dest_path, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Failed to copy " << src_path.string() << ": " << ec.message() << '\n';
    } else {
        std::cout << "  -> BACKED UP: " << dest_path.string() << '\n';
    }
}

// ============================================================
// Inotify: Add a single watch
// ============================================================
void add_watch(int inotify_fd, const fs::path& path, std::map<int, fs::path>& wd_map) {
    uint32_t mask = IN_ACCESS | IN_OPEN | IN_MODIFY | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | 
                    IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM;
                    
    int wd = inotify_add_watch(inotify_fd, path.c_str(), mask);
    if (wd < 0) {
        std::cerr << "inotify_add_watch failed for " << path.string() << ": " << strerror(errno) << '\n';
        return;
    }
    wd_map[wd] = path;
}

// ============================================================
// Inotify: Recursively add watches to a directory tree
// ============================================================
void watch_subtree(int inotify_fd, const fs::path& root, std::map<int, fs::path>& wd_map) {
    std::vector<fs::path> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        fs::path dir_path = stack.back();
        stack.pop_back();

        add_watch(inotify_fd, dir_path, wd_map);

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }

            std::string filename = entry.path().filename().string();
            // Avoid hidden directories (starts_with is C++20)
            if (entry.is_directory() && !filename.empty() && filename[0] != '.') {
                stack.push_back(entry.path());
            }
        }
    }
}

// ============================================================
// Resolve the user's home directory
// ============================================================
fs::path resolve_watch_path() {
    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        home = "/home";
    }

    if (strcmp(home, "/root") == 0) {
        const char* sudo_user = getenv("SUDO_USER");
        if (sudo_user && sudo_user[0] != '\0') {
            return fs::path("/home") / sudo_user;
        }
    }

    return fs::path(home);
}

// ============================================================
// MAIN
// ============================================================
int main(int argc, char* argv[]) {
    fs::path watch_path;

    if (argc > 1) {
        watch_path = argv[1];
    } else {
        watch_path = resolve_watch_path();
    }

    // Print info (std::format may not be available); use stream output instead
    std::cout << "Watch Path: " << watch_path.string() << '\n';
    std::cout << "Log File: " << LOG_FILE << '\n';
    std::cout << "Backup Directory: " << COPY_ROOT.string() << "\n\n";

    std::error_code ec;
    fs::create_directories(COPY_ROOT, ec);
    if (ec) {
        std::cerr << "Failed to create backup directory: " << ec.message() << '\n';
        return EXIT_FAILURE;
    }

    int inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        std::cerr << "inotify_init failed: " << strerror(errno) << '\n';
        return EXIT_FAILURE;
    }

    std::map<int, fs::path> wd_map;

    std::vector<std::string> personal_dirs = {
        "Desktop", "Documents", "Downloads", "Pictures", "Videos", "Music"
    };

    for (const auto& name : personal_dirs) {
        fs::path path = watch_path / name;
        if (fs::is_directory(path)) {
            std::cout << "Watching: " << path.string() << '\n';
            watch_subtree(inotify_fd, path, wd_map);
        }
    }

    std::cout << "\nMonitoring started. Press Ctrl+C to stop.\n\n";

    size_t buffer_size = (sizeof(struct inotify_event) + NAME_MAX + 1) * 1024;
    std::vector<char> buffer(buffer_size);

    while (true) {
        ssize_t length = read(inotify_fd, buffer.data(), buffer.size());
        if (length < 0) {
            if (errno == EINTR) continue;
            std::cerr << "read failed: " << strerror(errno) << '\n';
            break;
        }

        ssize_t offset = 0;
        while (offset < length) {
            struct inotify_event* event = reinterpret_cast<struct inotify_event*>(buffer.data() + offset);
            offset += sizeof(struct inotify_event) + event->len;

            auto it = wd_map.find(event->wd);
            if (it == wd_map.end()) continue;

            fs::path dir = it->second;
            fs::path full_path = dir;
            if (event->len > 0) {
                full_path /= event->name;
            }

            std::string s_full = full_path.string();
            std::string s_copy = COPY_ROOT.string();

            // Infinite loop prevention using C++20 starts_with()
            // std::string::starts_with is C++20; use rfind to check prefix for compatibility
            if (s_full == s_copy || s_full.rfind(s_copy + "/", 0) == 0) continue;
            if (s_full == LOG_FILE) continue;

            bool is_dir = (event->mask & IN_ISDIR) != 0;
            std::string_view event_name = get_event_name(event->mask);

            if (event->mask & (IN_ACCESS | IN_OPEN | IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM)) {
                std::cout << "[" << event_name << "] " << s_full << '\n';
                log_event(event_name, full_path);
            }

            if (!is_dir && (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO))) {
                backup_file(full_path, COPY_ROOT, watch_path);
            }

            if (is_dir && (event->mask & (IN_CREATE | IN_MOVED_TO))) {
                std::string filename = full_path.filename().string();
                if (!filename.empty() && filename[0] != '.') {
                    watch_subtree(inotify_fd, full_path, wd_map);
                }
            }
        }
    }

    close(inotify_fd);
    return EXIT_SUCCESS;
}