#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>

static std::string get_log_path() {
    return "/home/saif/Projects/Checkmate_Files/terminal_commands.txt";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <cwd> <command>\n";
        return EXIT_FAILURE;
    }

    std::string cwd = argv[1];
    std::string cmd = argv[2];
    for (int i = 3; i < argc; ++i) {
        cmd += " ";
        cmd += argv[i];
    }

    if (cmd.find_first_not_of(" \t\n\r") == std::string::npos) {
        return EXIT_SUCCESS;
    }

    std::string log_path = get_log_path();
    std::ofstream log_file(log_path, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log: " << log_path << "\n";
        return EXIT_FAILURE;
    }

    log_file << cwd << "   " << cmd << "\n";
    log_file.flush();

    return EXIT_SUCCESS;
}