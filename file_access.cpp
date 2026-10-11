#include <bits/stdc++.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <sys/wait.h>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================
// CONFIG
// ============================================================

const std::string HOME_DIR =
    "/home/saif";

const std::string LOG_FILE =
    "/home/saif/Projects/Checkmate_Files/file_access.log";


// ============================================================
// TIME
// ============================================================

std::string timestamp()
{
    auto now =
        std::chrono::system_clock::now();

    std::time_t t =
        std::chrono::system_clock::to_time_t(now);

    std::tm tm{};

    localtime_r(&t, &tm);

    std::ostringstream out;

    out << std::put_time(
        &tm,
        "%Y-%m-%d %H:%M:%S"
    );

    return out.str();
}


// ============================================================
// HIDDEN PATH CHECK
// ============================================================

bool hasHiddenComponent(
    const std::string& path)
{
    fs::path p(path);

    for (const auto& part : p)
    {
        std::string name =
            part.string();

        if (
            !name.empty() &&
            name[0] == '.' &&
            name != "." &&
            name != ".."
        )
        {
            return true;
        }
    }

    return false;
}


// ============================================================
// EXTENSION CHECK
// ============================================================

std::string getExtension(
    const std::string& path)
{
    fs::path p(path);

    std::string filename =
        p.filename().string();

    if (filename.empty())
        return "";

    // Ignore hidden files
    // Example:
    // .bashrc
    // .gitignore
    // .profile
    if (filename[0] == '.')
        return "";

    std::string ext =
        p.extension().string();

    if (ext.empty())
        return "";

    for (char& c : ext)
    {
        c = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(c)
            )
        );
    }

    return ext;
}


// ============================================================
// USER-FACING EXTENSIONS
//
// Only these extensions are considered useful.
// This removes things such as:
//
// .desktop
// .theme
// .ini
// .cache
// .xcu
// .pack
// .db
// .db-wal
// etc.
// ============================================================

bool isUserFile(
    const std::string& ext)
{
    static const std::unordered_set<std::string>
    allowed =
    {
        // -------------------------
        // Programming
        // -------------------------
        ".c",
        ".h",
        ".cc",
        ".cpp",
        ".cxx",
        ".hpp",
        ".java",
        ".py",
        ".pyw",
        ".js",
        ".ts",
        ".jsx",
        ".tsx",
        ".go",
        ".rs",
        ".php",
        ".rb",
        ".pl",
        ".sh",
        ".bash",
        ".zsh",
        ".fish",
        ".sql",
        ".asm",
        ".s",
        ".ino",

        // -------------------------
        // Text
        // -------------------------
        ".txt",
        ".md",
        ".rst",
        ".csv",
        ".log",

        // -------------------------
        // Documents
        // -------------------------
        ".pdf",
        ".doc",
        ".docx",
        ".odt",
        ".rtf",
        ".xls",
        ".xlsx",
        ".ods",
        ".ppt",
        ".pptx",
        ".odp",

        // -------------------------
        // Images
        // -------------------------
        ".jpg",
        ".jpeg",
        ".png",
        ".gif",
        ".bmp",
        ".webp",
        ".svg",
        ".ico",
        ".tif",
        ".tiff",

        // -------------------------
        // Audio
        // -------------------------
        ".mp3",
        ".wav",
        ".flac",
        ".ogg",
        ".m4a",

        // -------------------------
        // Video
        // -------------------------
        ".mp4",
        ".mkv",
        ".avi",
        ".mov",
        ".webm",

        // -------------------------
        // Archives
        // -------------------------
        ".zip",
        ".rar",
        ".7z",
        ".tar",
        ".gz",
        ".bz2",
        ".xz",

        // -------------------------
        // Web
        // -------------------------
        ".html",
        ".htm",
        ".css",

        // -------------------------
        // Data
        // -------------------------
        ".json",
        ".xml",
        ".yaml",
        ".yml",

        // -------------------------
        // Config intentionally
        // editable by user
        // -------------------------
        ".conf",
        ".cfg",

        // -------------------------
        // VM / Disk
        // -------------------------
        ".iso",
        ".img",
        ".vdi",
        ".vmdk",
        ".vhd"
    };

    return allowed.count(ext) > 0;
}


// ============================================================
// TEMPORARY FILE CHECK
// ============================================================

bool temporaryFile(
    const std::string& path)
{
    std::string filename =
        fs::path(path)
            .filename()
            .string();

    if (filename.empty())
        return true;


    // LibreOffice lock files
    if (
        filename.find(".~lock.") == 0
    )
    {
        return true;
    }


    // Vim
    if (
        filename.size() >= 4 &&
        filename.substr(
            filename.size() - 4
        ) == ".swp"
    )
    {
        return true;
    }


    if (
        filename.size() >= 4 &&
        filename.substr(
            filename.size() - 4
        ) == ".swo"
    )
    {
        return true;
    }


    // Backup files
    if (
        filename.back() == '~'
    )
    {
        return true;
    }


    // Common temporary extensions
    const std::string tempExt[] =
    {
        ".tmp",
        ".temp",
        ".bak",
        ".part",
        ".crdownload",
        ".lock",
        ".cache",
        ".journal"
    };


    std::string ext =
        getExtension(path);

    for (const auto& x : tempExt)
    {
        if (ext == x)
            return true;
    }


    return false;
}


// ============================================================
// BACKGROUND PROCESS FILTER
// ============================================================

bool ignoredProcess(
    const std::string& process)
{
    static const std::unordered_set<std::string>
    ignored =
    {
        // Desktop
        "xfdesktop",

        // File manager/background
        "thunar",
        "gvfsd",
        "gvfsd-fuse",
        "gvfsd-metadata",

        // Printing
        "CUPSManager",

        // Docker
        "docker",

        // Search/indexing
        "tracker",
        "tracker-miner-f",
        "tracker-miner-fs",
        "baloo_file",

        // Thumbnail generation
        "tumblerd",

        // Font handling
        "fc-cache",
        "fontconfig",

        // Desktop services
        "dbus-daemon",

        // System services
        "systemd",
        "systemd-logind",

        // Package related
        "packagekitd"
    };

    return ignored.count(process) > 0;
}


// ============================================================
// IGNORED DIRECTORY
// ============================================================

bool ignoredDirectory(
    const std::string& path)
{
    static const std::vector<std::string>
    ignored =
    {
        // Caches
        "/.cache/",
        "/.fontconfig/",
        "/.icons/",

        // Application configuration
        "/.config/",
        "/.local/share/",

        // Browser internals
        "/.mozilla/",
        "/.chromium/",
        "/.config/google-chrome/",
        "/.config/chromium/",

        // VS Code internals
        "/.vscode/",
        "/.config/Code/",
        "/.cache/vscode-cpptools/",

        // Git internals
        "/.git/",

        // Docker
        "/.docker/",

        // Trash
        "/.local/share/Trash/",

        // Your generated/monitoring files
        "/Projects/Checkmate_Files/"
    };


    for (const auto& dir : ignored)
    {
        if (
            path.find(dir) !=
            std::string::npos
        )
        {
            return true;
        }
    }

    return false;
}


// ============================================================
// PATH FILTER
// ============================================================

bool ignoredPath(
    const std::string& path)
{
    // Must be inside user's home
    if (
        path.rfind(
            HOME_DIR + "/",
            0
        ) != 0
    )
    {
        return true;
    }


    // Ignore hidden files/directories
    //
    // This removes:
    //
    // ~/.cache/...
    // ~/.config/...
    // ~/.local/...
    // ~/.icons/...
    // etc.
    //
    if (hasHiddenComponent(path))
        return true;


    // Ignore known application internals
    if (ignoredDirectory(path))
        return true;


    // Don't monitor our own log
    if (
        path ==
        LOG_FILE
    )
    {
        return true;
    }


    return false;
}


// ============================================================
// WRITE LOG
// ============================================================

void writeLog(
    const std::string& pid,
    const std::string& process,
    const std::string& path)
{
    if (path.empty())
        return;


    // -----------------------------------------
    // Path filtering
    // -----------------------------------------

    if (ignoredPath(path))
        return;


    // -----------------------------------------
    // Process filtering
    // -----------------------------------------

    if (ignoredProcess(process))
        return;


    // -----------------------------------------
    // Temporary files
    // -----------------------------------------

    if (temporaryFile(path))
        return;


    // -----------------------------------------
    // Extension
    // -----------------------------------------

    std::string ext =
        getExtension(path);

    if (ext.empty())
        return;


    // -----------------------------------------
    // Only user-facing extensions
    // -----------------------------------------

    if (!isUserFile(ext))
        return;


    // ========================================================
    // UNIQUE FOR 5 MINUTES
    // ========================================================

    static std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point
    > lastSeen;


    auto now =
        std::chrono::steady_clock::now();


    auto it =
        lastSeen.find(path);


    if (it != lastSeen.end())
    {
        auto seconds =
            std::chrono::duration_cast<
                std::chrono::seconds
            >(
                now - it->second
            ).count();


        if (seconds < 300)
            return;
    }


    lastSeen[path] = now;


    // ========================================================
    // WRITE
    // ========================================================

    std::ofstream log(
        LOG_FILE,
        std::ios::app
    );


    if (!log)
    {
        std::cerr
            << "Cannot open log file: "
            << LOG_FILE
            << '\n';

        return;
    }


    log
        << timestamp()
        << " | "
        << process
        << " | PID="
        << pid
        << " | "
        << path
        << '\n';


    std::cout
        << "[FILE OPENED] "
        << path
        << " | "
        << process
        << " | PID="
        << pid
        << '\n';
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    std::cout
        << "========================================\n"
        << "       eBPF USER FILE MONITOR\n"
        << "========================================\n"
        << "Watching : "
        << HOME_DIR
        << '\n'
        << "Log      : "
        << LOG_FILE
        << '\n'
        << "========================================\n\n";


    fs::create_directories(
        fs::path(LOG_FILE).parent_path()
    );


    // ========================================================
    // BPFTRACE
    // ========================================================

    const char* command = R"EOF(
sudo bpftrace -e '
tracepoint:syscalls:sys_enter_openat
/str(args->filename) != ""/
{
    printf("FILECHECK|%d|%s|%s\n",
           pid,
           comm,
           str(args->filename));
}
'
)EOF";


    FILE* pipe =
        popen(
            command,
            "r"
        );


    if (!pipe)
    {
        std::cerr
            << "Failed to start bpftrace.\n";

        return 1;
    }


    char buffer[4096];


    while (
        fgets(
            buffer,
            sizeof(buffer),
            pipe
        )
    )
    {
        std::string line(buffer);


        if (
            line.rfind(
                "FILECHECK|",
                0
            ) != 0
        )
        {
            continue;
        }


        std::stringstream ss(line);


        std::string prefix;
        std::string pid;
        std::string process;
        std::string path;


        std::getline(
            ss,
            prefix,
            '|'
        );


        std::getline(
            ss,
            pid,
            '|'
        );


        std::getline(
            ss,
            process,
            '|'
        );


        std::getline(
            ss,
            path
        );


        while (
            !path.empty() &&
            (
                path.back() == '\n' ||
                path.back() == '\r'
            )
        )
        {
            path.pop_back();
        }


        if (path.empty())
            continue;


        writeLog(
            pid,
            process,
            path
        );
    }


    int status =
        pclose(pipe);


    if (WIFEXITED(status))
    {
        std::cerr
            << "bpftrace exited with status "
            << WEXITSTATUS(status)
            << '\n';
    }


    return 0;
}