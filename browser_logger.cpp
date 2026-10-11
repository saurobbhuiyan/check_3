#include <cstdlib>
#include <iostream>

int main() {
    std::cout << "Starting network logger...\n";

    std::string cmd =
        "sudo tshark -i wlan0 "
        "-Y \"ip.addr == 192.168.0.182 && (dns.qry.name || tls.handshake.extensions_server_name)\" "
        //"-Y \"ip.addr == 192.168.0.182 && dns.qry.name\" "
        "-T fields "
        "-e frame.time "
        "-e ip.dst "
        "-e dns.qry.name "
        "-e tls.handshake.extensions_server_name "
        ">> /home/saurob/Projects/Checkmate_Files/raw.log";

    system(cmd.c_str());

    return 0;
}