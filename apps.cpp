#include <arpa/inet.h>
#include <linux/connector.h>
#include <linux/netlink.h>
#include <linux/cn_proc.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ============================================================
// GUI APPLICATION DATABASE
// executable name -> application name
// ============================================================

static std::map<std::string, std::string> gui_apps;


// ============================================================
// Trim
// ============================================================

static std::string trim(const std::string &s)
{
    size_t a = 0;

    while (a < s.size() &&
           std::isspace((unsigned char)s[a]))
        a++;

    size_t b = s.size();

    while (b > a &&
           std::isspace((unsigned char)s[b - 1]))
        b--;

    return s.substr(a, b - a);
}


// ============================================================
// Lowercase
// ============================================================

static std::string lower_string(std::string s)
{
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        }
    );

    return s;
}


// ============================================================
// Basename
// ============================================================

static std::string basename_from_path(const std::string &path)
{
    size_t pos = path.find_last_of('/');

    if (pos == std::string::npos)
        return path;

    return path.substr(pos + 1);
}


// ============================================================
// Remove quotes
// ============================================================

static std::string remove_quotes(std::string s)
{
    if (s.size() >= 2)
    {
        if ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\''))
        {
            return s.substr(1, s.size() - 2);
        }
    }

    return s;
}


// ============================================================
// Parse Exec=
//
// Example:
//
// Exec=/usr/bin/wireshark %F
//
// We only need the executable:
// /usr/bin/wireshark
// ============================================================

static std::string parse_exec_program(std::string exec)
{
    exec = trim(exec);

    if (exec.empty())
        return "";

    if (exec[0] == '"')
    {
        size_t end = exec.find('"', 1);

        if (end != std::string::npos)
            return exec.substr(1, end - 1);
    }

    size_t space = exec.find_first_of(" \t");

    if (space != std::string::npos)
        exec = exec.substr(0, space);

    return remove_quotes(exec);
}


// ============================================================
// Load desktop files
// ============================================================

static void load_desktop_apps()
{
    std::vector<std::string> dirs =
    {
        "/usr/share/applications",
        "/usr/local/share/applications",
        "/var/lib/flatpak/exports/share/applications",

        std::string(getenv("HOME") ? getenv("HOME") : "") +
            "/.local/share/applications",

        std::string(getenv("HOME") ? getenv("HOME") : "") +
            "/.local/share/flatpak/exports/share/applications"
    };


    for (const auto &dir : dirs)
    {
        if (dir.empty())
            continue;

        if (!fs::exists(dir))
            continue;

        try
        {
            for (const auto &entry :
                 fs::directory_iterator(dir))
            {
                if (!entry.is_regular_file())
                    continue;

                if (entry.path().extension() != ".desktop")
                    continue;


                std::ifstream file(entry.path());

                if (!file.is_open())
                    continue;


                std::string app_name;
                std::string exec_line;

                bool desktop_entry = false;

                std::string line;

                while (std::getline(file, line))
                {
                    line = trim(line);

                    if (line == "[Desktop Entry]")
                    {
                        desktop_entry = true;
                        continue;
                    }

                    if (!desktop_entry)
                        continue;

                    if (line.rfind("Name=", 0) == 0)
                    {
                        if (app_name.empty())
                            app_name = line.substr(5);
                    }

                    else if (line.rfind("Exec=", 0) == 0)
                    {
                        exec_line = line.substr(5);
                    }
                }


                if (app_name.empty() ||
                    exec_line.empty())
                    continue;


                std::string program =
                    parse_exec_program(exec_line);

                if (program.empty())
                    continue;


                std::string executable =
                    basename_from_path(program);


                // -----------------------------------------------
                // Ignore obvious wrapper programs
                // -----------------------------------------------

                if (executable == "sh" ||
                    executable == "bash" ||
                    executable == "dash" ||
                    executable == "env")
                {
                    continue;
                }


                // -----------------------------------------------
                // Store executable
                // -----------------------------------------------

                gui_apps[executable] = app_name;


                // ------------------------------------------------
                // Also store lowercase version.
                // ------------------------------------------------

                gui_apps[
                    lower_string(executable)
                ] = app_name;
            }
        }

        catch (...)
        {
            continue;
        }
    }
}


// ============================================================
// Read /proc/PID/exe
// ============================================================

static std::string read_proc_exe(pid_t pid)
{
    char path[64];
    char exe[4096];

    snprintf(
        path,
        sizeof(path),
        "/proc/%d/exe",
        pid
    );

    ssize_t len =
        readlink(
            path,
            exe,
            sizeof(exe) - 1
        );

    if (len <= 0)
        return "";

    exe[len] = '\0';

    return std::string(exe);
}


// ============================================================
// Read /proc/PID/cmdline
// ============================================================

static std::string read_proc_cmdline(pid_t pid)
{
    char path[64];

    snprintf(
        path,
        sizeof(path),
        "/proc/%d/cmdline",
        pid
    );

    std::ifstream file(
        path,
        std::ios::binary
    );

    if (!file.is_open())
        return "";


    std::string result;
    std::string part;

    while (std::getline(file, part, '\0'))
    {
        if (!result.empty())
            result += ' ';

        result += part;
    }

    return result;
}


// ============================================================
// AppImage
// ============================================================

static bool is_appimage(const std::string &exe)
{
    if (exe.empty())
        return false;

    if (exe.find(".AppImage") != std::string::npos)
        return true;

    if (exe.find("AppImage") != std::string::npos)
        return true;

    if (exe.find("/tmp/.mount_") != std::string::npos)
        return true;

    if (exe.find(".mount_") != std::string::npos)
        return true;

    return false;
}


// ============================================================
// AppImage name
// ============================================================

static std::string appimage_name(const std::string &exe)
{
    std::string name =
        basename_from_path(exe);

    size_t pos =
        name.find(".AppImage");

    if (pos != std::string::npos)
        name = name.substr(0, pos);

    return name;
}


// ============================================================
// Burp detection
// ============================================================

static bool is_burp(const std::string &cmdline)
{
    std::string s =
        lower_string(cmdline);

    if (s.find("burpsuite") != std::string::npos)
        return true;

    if (s.find("burp.jar") != std::string::npos)
        return true;

    if (s.find("burp") != std::string::npos &&
        s.find("java") != std::string::npos)
        return true;

    return false;
}


// ============================================================
// Special known GUI executables
//
// This is NOT the main detection method.
// These are applications whose desktop files commonly use
// wrappers or unusual Exec entries.
// ============================================================

static std::string known_gui_app(
    const std::string &exe)
{
    static const std::map<std::string, std::string> known =
    {
        {"sublime_text", "Sublime Text"},
        {"wireshark", "Wireshark"},
        {"ghb", "HandBrake"},
        {"telegram-desktop", "Telegram"},
        {"telegram", "Telegram"},
        {"qterminal", "QTerminal"},
        {"firefox", "Firefox"},
        {"chromium", "Chromium"},
        {"chromium-browser", "Chromium"},
        {"code", "Visual Studio Code"},
        {"code-oss", "Visual Studio Code"},
        {"vlc", "VLC"},
        {"obs", "OBS Studio"},
        {"audacity", "Audacity"},
        {"gimp", "GIMP"},
        {"inkscape", "Inkscape"},
        {"thunar", "Thunar"}
    };


    auto it = known.find(exe);

    if (it != known.end())
        return it->second;

    return "";
}


// ============================================================
// Ignore helper processes
// ============================================================

static bool ignored_helper(
    const std::string &exe)
{
    static const std::set<std::string> ignored =
    {
        "sh",
        "bash",
        "zsh",
        "dash",

        "ps",
        "sed",
        "cat",
        "grep",
        "sleep",
        "which",
        "tput",
        "dircolors",
        "dirname",

        "mount",
        "umount",
        "fusermount",
        "fusermount3",

        "modprobe",
        "sysctl",

        "iptables",
        "ip6tables",

        "dumpcap",
        "androiddump",
        "sshdump",
        "ciscodump",
        "udpdump",
        "wifidump",
        "randpktdump",

        "vamp-plugin-load-checker",
        "piper-vamp-simple-server",

        "jspawnhelper",

        "xdg-open",
        "exo-open",
        "xfce4-mime-helper",

        "plugin_host-3.3",
        "plugin_host-3.8",

        "crash_handler",
        "update-desktop-database",

        "ld-linux-x86-64.so.2",
        "ld-linux.so.2"
    };


    return ignored.count(exe) > 0;
}


// ============================================================
// Netlink process connector
// ============================================================

static bool setup_proc_connector(
    int &sock_fd)
{
    sock_fd =
        socket(
            PF_NETLINK,
            SOCK_DGRAM,
            NETLINK_CONNECTOR
        );

    if (sock_fd < 0)
    {
        perror("socket");
        return false;
    }


    struct sockaddr_nl sa;

    memset(
        &sa,
        0,
        sizeof(sa)
    );

    sa.nl_family = AF_NETLINK;
    sa.nl_groups = CN_IDX_PROC;


    if (bind(
            sock_fd,
            reinterpret_cast<struct sockaddr *>(&sa),
            sizeof(sa)
        ) < 0)
    {
        perror("bind");

        close(sock_fd);

        return false;
    }


    char buffer[
        NLMSG_SPACE(
            sizeof(struct cn_msg) +
            sizeof(enum proc_cn_mcast_op)
        )
    ];

    memset(
        buffer,
        0,
        sizeof(buffer)
    );


    struct nlmsghdr *nlh =
        reinterpret_cast<struct nlmsghdr *>(
            buffer
        );


    struct cn_msg *cn_hdr =
        reinterpret_cast<struct cn_msg *>(
            NLMSG_DATA(nlh)
        );


    enum proc_cn_mcast_op *op =
        reinterpret_cast<enum proc_cn_mcast_op *>(
            cn_hdr->data
        );


    nlh->nlmsg_len =
        NLMSG_LENGTH(
            sizeof(struct cn_msg) +
            sizeof(enum proc_cn_mcast_op)
        );

    nlh->nlmsg_type = NLMSG_DONE;
    nlh->nlmsg_flags = 0;
    nlh->nlmsg_seq = 0;
    nlh->nlmsg_pid = getpid();


    cn_hdr->id.idx = CN_IDX_PROC;
    cn_hdr->id.val = CN_VAL_PROC;

    cn_hdr->len =
        sizeof(enum proc_cn_mcast_op);


    *op = PROC_CN_MCAST_LISTEN;


    if (send(
            sock_fd,
            buffer,
            nlh->nlmsg_len,
            0
        ) < 0)
    {
        perror("send");

        close(sock_fd);

        return false;
    }

    return true;
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // ---------------------------------------------------------
    // Build GUI application database
    // ---------------------------------------------------------

    load_desktop_apps();


    // ---------------------------------------------------------
    // Setup process listener
    // ---------------------------------------------------------

    int sock_fd;

    if (!setup_proc_connector(sock_fd))
        return EXIT_FAILURE;


    // ---------------------------------------------------------
    // Log
    // ---------------------------------------------------------

    std::ofstream log_file(
        "/home/saif/Projects/Checkmate_Files/app.log",
        std::ios::app
    );


    if (!log_file.is_open())
    {
        perror("Unable to open log file");

        close(sock_fd);

        return EXIT_FAILURE;
    }


    // ---------------------------------------------------------
    // Currently logged applications
    //
    // app name -> PID
    // ---------------------------------------------------------

    std::map<std::string, pid_t> active_apps;


    // =========================================================
    // EVENT LOOP
    // =========================================================

    while (true)
    {
        char buffer[8192];


        ssize_t len =
            recv(
                sock_fd,
                buffer,
                sizeof(buffer),
                0
            );


        if (len <= 0)
            continue;


        struct nlmsghdr *nlh =
            reinterpret_cast<struct nlmsghdr *>(
                buffer
            );


        while (NLMSG_OK(nlh, len))
        {
            struct cn_msg *cn_hdr =
                reinterpret_cast<struct cn_msg *>(
                    NLMSG_DATA(nlh)
                );


            if (cn_hdr->id.idx == CN_IDX_PROC &&
                cn_hdr->id.val == CN_VAL_PROC)
            {
                struct proc_event *ev =
                    reinterpret_cast<struct proc_event *>(
                        cn_hdr->data
                    );


                // =================================================
                // PROCESS EXEC
                // =================================================

                if (ev->what == PROC_EVENT_EXEC)
                {
                    pid_t pid =
                        ev->event_data.exec.process_pid;


                    std::string exe =
                        read_proc_exe(pid);


                    if (exe.empty())
                    {
                        nlh = NLMSG_NEXT(nlh, len);
                        continue;
                    }


                    std::string executable =
                        basename_from_path(exe);


                    // ------------------------------------------------
                    // Ignore obvious helpers
                    // ------------------------------------------------

                    if (ignored_helper(executable))
                    {
                        nlh = NLMSG_NEXT(nlh, len);
                        continue;
                    }


                    // ------------------------------------------------
                    // Read command line
                    // ------------------------------------------------

                    std::string cmdline =
                        read_proc_cmdline(pid);


                    std::string app_name;


                    // =================================================
                    // 1. Desktop file match
                    // =================================================

                    auto it =
                        gui_apps.find(executable);


                    if (it != gui_apps.end())
                    {
                        app_name = it->second;
                    }


                    // =================================================
                    // 2. Case-insensitive desktop match
                    // =================================================

                    if (app_name.empty())
                    {
                        std::string lower_exe =
                            lower_string(executable);


                        auto it2 =
                            gui_apps.find(lower_exe);


                        if (it2 != gui_apps.end())
                            app_name = it2->second;
                    }


                    // =================================================
                    // 3. Known GUI application
                    // =================================================

                    if (app_name.empty())
                    {
                        app_name =
                            known_gui_app(executable);
                    }


                    // =================================================
                    // 4. AppImage
                    // =================================================

                    if (app_name.empty() &&
                        is_appimage(exe))
                    {
                        app_name =
                            appimage_name(exe);
                    }


                    // =================================================
                    // 5. Burp Suite
                    // =================================================

                    if (app_name.empty() &&
                        (executable == "java" ||
                         executable == "javaw"))
                    {
                        if (is_burp(cmdline))
                        {
                            app_name =
                                "Burp Suite";
                        }
                    }


                    // =================================================
                    // Nothing recognized
                    // =================================================

                    if (app_name.empty())
                    {
                        nlh = NLMSG_NEXT(nlh, len);
                        continue;
                    }


                    // =================================================
                    // Don't log same application repeatedly
                    // while it is already running.
                    // =================================================

                    if (active_apps.count(app_name))
                    {
                        nlh = NLMSG_NEXT(nlh, len);
                        continue;
                    }


                    active_apps[app_name] = pid;


                    // =================================================
                    // WRITE LOG
                    // =================================================

                    log_file
                        << "APP OPENED: "
                        << app_name
                        << std::endl;


                    log_file.flush();
                }


                // =================================================
                // PROCESS EXIT
                // =================================================

                else if (
                    ev->what == PROC_EVENT_EXIT)
                {
                    pid_t pid =
                        ev->event_data.exit.process_pid;


                    for (auto it =
                             active_apps.begin();
                         it != active_apps.end();)
                    {
                        if (it->second == pid)
                        {
                            it =
                                active_apps.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
            }


            nlh = NLMSG_NEXT(nlh, len);
        }
    }


    log_file.close();

    close(sock_fd);

    return EXIT_SUCCESS;
}