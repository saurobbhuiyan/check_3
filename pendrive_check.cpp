#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <ctime>
#include <cstring>

#include <libudev.h>
#include <unistd.h>
#include <sys/select.h>

using namespace std;


// ------------------------------------------------------------
// Configuration
// ------------------------------------------------------------

const string LOG_FILE =
    "/home/saif/Projects/Checkmate_Files/usb.log";


// ------------------------------------------------------------
// Information about a connected USB device
// ------------------------------------------------------------

struct DeviceInfo
{
    time_t connectedTime;

    string model;
    string vendor;
    string serial;
};


// Device node -> device information
map<string, DeviceInfo> connectedDevices;


// ------------------------------------------------------------
// Time formatting
// ------------------------------------------------------------

string bdTimeString(time_t ts)
{
    char buffer[64];

    tm localTime{};

    localtime_r(&ts, &localTime);

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d %H:%M:%S",
        &localTime
    );

    return string(buffer);
}


// ------------------------------------------------------------
// Write a message to both console and log file
// ------------------------------------------------------------

void logLine(const string& text)
{
    cout << text << endl;

    ofstream log(
        LOG_FILE,
        ios::app
    );

    if (log)
    {
        log << text << "\n";
    }
}


// ------------------------------------------------------------
// Get a udev property safely
// ------------------------------------------------------------

string getProperty(
    struct udev_device* dev,
    const char* property
)
{
    const char* value =
        udev_device_get_property_value(
            dev,
            property
        );

    if (value)
        return string(value);

    return "";
}


// ------------------------------------------------------------
// Check whether this is a USB storage device
// ------------------------------------------------------------

bool isUSBStorage(
    struct udev_device* dev
)
{
    if (!dev)
        return false;


    /*
        Check that the device belongs to
        the block subsystem.
    */

    const char* subsystem =
        udev_device_get_subsystem(dev);

    if (!subsystem ||
        strcmp(subsystem, "block") != 0)
    {
        return false;
    }


    /*
        Find the parent USB device.

        If there is no USB parent,
        this isn't a USB storage device.
    */

    struct udev_device* usbParent =
        udev_device_get_parent_with_subsystem_devtype(
            dev,
            "usb",
            "usb_device"
        );

    return usbParent != nullptr;
}


// ------------------------------------------------------------
// Get device information
// ------------------------------------------------------------

DeviceInfo getDeviceInfo(
    struct udev_device* dev,
    time_t connectedTime
)
{
    DeviceInfo info;

    info.connectedTime = connectedTime;


    const char* model =
        udev_device_get_property_value(
            dev,
            "ID_MODEL"
        );

    const char* vendor =
        udev_device_get_property_value(
            dev,
            "ID_VENDOR"
        );

    const char* serial =
        udev_device_get_property_value(
            dev,
            "ID_SERIAL_SHORT"
        );


    if (model)
        info.model = model;

    if (vendor)
        info.vendor = vendor;

    if (serial)
        info.serial = serial;


    return info;
}


// ------------------------------------------------------------
// Log device connection
// ------------------------------------------------------------

void logConnected(
    const string& devnode,
    const DeviceInfo& info
)
{
    ofstream log(
        LOG_FILE,
        ios::app
    );

    if (!log)
    {
        cerr << "Cannot open log file: "
             << LOG_FILE << endl;

        return;
    }


    log << "\n";
    log << "[CONNECTED]\n";

    log << "Device    : "
        << devnode << "\n";

    if (!info.vendor.empty())
    {
        log << "Vendor    : "
            << info.vendor << "\n";
    }

    if (!info.model.empty())
    {
        log << "Model     : "
            << info.model << "\n";
    }

    if (!info.serial.empty())
    {
        log << "Serial    : "
            << info.serial << "\n";
    }

    log << "Connected : "
        << bdTimeString(info.connectedTime)
        << "\n";

    log << "\n";


    cout << "[CONNECTED]\n";

    cout << "Device    : "
         << devnode << "\n";

    if (!info.vendor.empty())
    {
        cout << "Vendor    : "
             << info.vendor << "\n";
    }

    if (!info.model.empty())
    {
        cout << "Model     : "
             << info.model << "\n";
    }

    if (!info.serial.empty())
    {
        cout << "Serial    : "
             << info.serial << "\n";
    }

    cout << "Connected : "
         << bdTimeString(info.connectedTime)
         << "\n\n";
}


// ------------------------------------------------------------
// Log device removal
// ------------------------------------------------------------

void logRemoved(
    const string& devnode,
    const DeviceInfo& info,
    time_t removedTime
)
{
    long long duration =
        removedTime - info.connectedTime;


    ofstream log(
        LOG_FILE,
        ios::app
    );

    if (!log)
    {
        cerr << "Cannot open log file: "
             << LOG_FILE << endl;

        return;
    }


    log << "\n";
    log << "[REMOVED]\n";

    log << "Device      : "
        << devnode << "\n";

    if (!info.vendor.empty())
    {
        log << "Vendor      : "
            << info.vendor << "\n";
    }

    if (!info.model.empty())
    {
        log << "Model       : "
            << info.model << "\n";
    }

    if (!info.serial.empty())
    {
        log << "Serial      : "
            << info.serial << "\n";
    }

    log << "Connected   : "
        << bdTimeString(info.connectedTime)
        << "\n";

    log << "Disconnected: "
        << bdTimeString(removedTime)
        << "\n";

    log << "Duration    : "
        << duration
        << " seconds\n";

    log << "\n";


    cout << "[REMOVED]\n";

    cout << "Device      : "
         << devnode << "\n";

    cout << "Connected   : "
         << bdTimeString(info.connectedTime)
         << "\n";

    cout << "Disconnected: "
         << bdTimeString(removedTime)
         << "\n";

    cout << "Duration    : "
         << duration
         << " seconds\n\n";
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    time_t programStart =
        time(nullptr);


    // --------------------------------------------------------
    // Create udev context
    // --------------------------------------------------------

    struct udev* udev =
        udev_new();

    if (!udev)
    {
        cerr << "Failed to create udev context\n";
        return 1;
    }


    // --------------------------------------------------------
    // Startup information
    // --------------------------------------------------------

    cout << "====================================\n";
    cout << " USB Monitoring Started\n";
    cout << " Start Time (BD): "
         << bdTimeString(programStart)
         << "\n";
    cout << " Log File: "
         << LOG_FILE
         << "\n";
    cout << "====================================\n\n";


    ofstream log(
        LOG_FILE,
        ios::app
    );

    if (log)
    {
        log << "\n";
        log << "====================================\n";
        log << "USB Monitoring Started\n";
        log << "Start Time: "
            << bdTimeString(programStart)
            << "\n";
        log << "====================================\n";
    }


    // --------------------------------------------------------
    // Detect USB storage devices already connected
    // --------------------------------------------------------

    struct udev_enumerate* enumerate =
        udev_enumerate_new(udev);

    udev_enumerate_add_match_subsystem(
        enumerate,
        "block"
    );

    udev_enumerate_scan_devices(
        enumerate
    );


    struct udev_list_entry* devices =
        udev_enumerate_get_list_entry(
            enumerate
        );

    struct udev_list_entry* entry;


    udev_list_entry_foreach(
        entry,
        devices
    )
    {
        const char* path =
            udev_list_entry_get_name(
                entry
            );


        struct udev_device* dev =
            udev_device_new_from_syspath(
                udev,
                path
            );


        if (!dev)
            continue;


        const char* devnode =
            udev_device_get_devnode(dev);


        if (devnode &&
            isUSBStorage(dev))
        {
            DeviceInfo info =
                getDeviceInfo(
                    dev,
                    programStart
                );


            connectedDevices[devnode] =
                info;


            logConnected(
                devnode,
                info
            );
        }


        udev_device_unref(dev);
    }


    udev_enumerate_unref(
        enumerate
    );


    // --------------------------------------------------------
    // Create udev monitor
    // --------------------------------------------------------

    struct udev_monitor* monitor =
        udev_monitor_new_from_netlink(
            udev,
            "udev"
        );


    if (!monitor)
    {
        cerr << "Failed to create udev monitor\n";

        udev_unref(udev);

        return 1;
    }


    udev_monitor_filter_add_match_subsystem_devtype(
        monitor,
        "block",
        nullptr
    );


    udev_monitor_enable_receiving(
        monitor
    );


    int fd =
        udev_monitor_get_fd(
            monitor
        );


    // --------------------------------------------------------
    // Monitoring loop
    // --------------------------------------------------------

    while (true)
    {
        fd_set fds;

        FD_ZERO(&fds);

        FD_SET(
            fd,
            &fds
        );


        int ret =
            select(
                fd + 1,
                &fds,
                nullptr,
                nullptr,
                nullptr
            );


        if (ret <= 0)
            continue;


        if (!FD_ISSET(fd, &fds))
            continue;


        struct udev_device* dev =
            udev_monitor_receive_device(
                monitor
            );


        if (!dev)
            continue;


        const char* action =
            udev_device_get_action(dev);


        const char* devnode =
            udev_device_get_devnode(dev);


        if (devnode &&
            isUSBStorage(dev))
        {
            string node(devnode);


            // ------------------------------------------------
            // USB storage connected
            // ------------------------------------------------

            if (action &&
                strcmp(action, "add") == 0)
            {
                time_t now =
                    time(nullptr);


                DeviceInfo info =
                    getDeviceInfo(
                        dev,
                        now
                    );


                connectedDevices[node] =
                    info;


                logConnected(
                    node,
                    info
                );
            }


            // ------------------------------------------------
            // USB storage removed
            // ------------------------------------------------

            else if (
                action &&
                strcmp(action, "remove") == 0
            )
            {
                time_t now =
                    time(nullptr);


                auto it =
                    connectedDevices.find(
                        node
                    );


                if (it !=
                    connectedDevices.end())
                {
                    logRemoved(
                        node,
                        it->second,
                        now
                    );


                    connectedDevices.erase(
                        it
                    );
                }
            }
        }


        udev_device_unref(dev);
    }


    udev_monitor_unref(monitor);

    udev_unref(udev);

    return 0;
}