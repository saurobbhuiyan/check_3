#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <unistd.h>

struct PCloseDeleter {
    void operator()(FILE* ptr) const {
        if (ptr) pclose(ptr);
    }
};

std::string get_clipboard_content(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, PCloseDeleter> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        std::cerr << "[DEBUG] Failed to open pipe for command.\n";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main() {
    const char* session = getenv("XDG_SESSION_TYPE");
    std::cout << "[DEBUG] XDG_SESSION_TYPE = " << (session ? session : "NULL") << "\n";

    if (!session) {
        std::cerr << "Cannot determine session type.\n";
        return 1;
    }

    std::string type(session);
    const std::string logfile = "/home/saif/Projects/Checkmate_Files/clipboard.log";
    std::cout << "[DEBUG] Target logfile = " << logfile << "\n";

    std::string cmd;
    std::string tool_name;
    
    if (type == "x11") {
        std::cout << "[DEBUG] Detected X11\n";
        cmd = "xclip -o -selection clipboard 2>/dev/null";
        tool_name = "xclip";
    } else if (type == "wayland") {
        std::cout << "[DEBUG] Detected Wayland\n";
        cmd = "wl-paste 2>/dev/null";
        tool_name = "wl-paste";
    } else {
        std::cerr << "Unknown session type: " << type << "\n";
        return 1;
    }

    // Check if tool is installed
    std::string check_cmd = "which " + tool_name + " > /dev/null 2>&1";
    if (system(check_cmd.c_str()) != 0) {
        std::cerr << "[ERROR] '" << tool_name << "' is NOT installed or not in PATH.\n";
        std::cerr << "[ACTION] Please run: sudo apt install " << tool_name << "\n";
        return 1;
    }
    std::cout << "[DEBUG] Tool '" << tool_name << "' found successfully.\n";

    std::string prev_content = "__INITIAL_EMPTY_SENTINEL__";
    std::cout << "[INFO] Monitoring started. Copy some text now, then watch this terminal.\n";
    std::cout << "[INFO] Press Ctrl+C to stop.\n";

    while (true) {
        std::string current_content = get_clipboard_content(cmd);
        
        if (current_content != prev_content) {
            std::cout << "[DEBUG] Clipboard changed! Length: " << current_content.length() << " bytes.\n";
            
            if (!current_content.empty()) {
                FILE* file = fopen(logfile.c_str(), "a");
                if (file) {
                    std::cout << "[DEBUG] File opened. Writing content...\n";
                    fputs(current_content.c_str(), file);
                    if (current_content.back() != '\n') {
                        fputs("\n", file);
                    }
                    fputs("------------------------------\n", file);
                    fclose(file);
                    std::cout << "[SUCCESS] Logged to file!\n";
                } else {
                    std::cerr << "[ERROR] Failed to open log file: " << logfile << "\n";
                    perror("[ERROR] System says");
                }
            } else {
                std::cout << "[DEBUG] Clipboard is empty, skipping log.\n";
            }
            prev_content = current_content;
        }

        sleep(1);
    }

    return 0;
}