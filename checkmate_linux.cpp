#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

static const std::string BASE =
    "/home/saif/Projects/Checkmate_Files";

static volatile sig_atomic_t running = 1;

void signalHandler(int)
{
    running = 0;
}

// ------------------------------------------------------------
// Create log directory
// ------------------------------------------------------------

void prepare()
{
    std::string cmd =
        "mkdir -p \"" + BASE + "\"";

    std::system(cmd.c_str());
}

// ------------------------------------------------------------
// Run a collector
// ------------------------------------------------------------

void runCollector(
    const std::string& name,
    const std::string& command)
{
    std::cout
        << "[CHECKMATE] Starting "
        << name
        << "...\n";

    int result =
        std::system(command.c_str());

    std::cerr
        << "[CHECKMATE] "
        << name
        << " stopped. exit="
        << result
        << "\n";
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout
        << "============================================\n"
        << "              CHECKMATE MONITOR\n"
        << "============================================\n"
        << "Log directory:\n"
        << BASE
        << "\n"
        << "============================================\n\n";

    prepare();

    /*
     * Each collector is kept independent.
     *
     * Replace these paths with the actual compiled
     * executable names on your system.
     */

    std::vector<std::thread> collectors;

    // --------------------------------------------------------
    // APPLICATION MONITOR
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "Application Monitor",
            "/home/saif/Projects/Checkmate/apps"
        );
    });


    // --------------------------------------------------------
    // FILE MONITOR
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "File Monitor",
            "/home/saif/Projects/Checkmate/file_access"
        );
    });


    // --------------------------------------------------------
    // USB / PENDRIVE MONITOR
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "USB Monitor",
            "/home/saif/Projects/Checkmate/usb_logger"
        );
    });


    // --------------------------------------------------------
    // TERMINAL MONITOR
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "Terminal Monitor",
            "/home/saif/Projects/Checkmate/cmd_logger"
        );
    });


    // --------------------------------------------------------
    // NETWORK MONITOR
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "Network Monitor",
            "/home/saif/Projects/Checkmate/browser_logger"
        );
    });


    // --------------------------------------------------------
    // CLIPBOARD
    //
    // Use a metadata-only collector rather than storing
    // the actual clipboard contents.
    // --------------------------------------------------------

    collectors.emplace_back([]()
    {
        runCollector(
            "Clipboard Monitor",
            "/home/saif/Projects/Checkmate/clip_logger"
        );
    });


    // --------------------------------------------------------
    // Supervisor
    // --------------------------------------------------------

    while (running)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );
    }


    std::cout
        << "\n[CHECKMATE] Stopping...\n";


    /*
     * The individual programs currently run their own
     * infinite loops, so Ctrl+C handling should eventually
     * be added to each collector as well.
     */

    for (auto& thread : collectors)
    {
        if (thread.joinable())
            thread.detach();
    }


    std::cout
        << "[CHECKMATE] Stopped.\n";

    return 0;
}