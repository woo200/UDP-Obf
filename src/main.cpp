/**********************************************************************
 * Copyright (c) 2023 John Woo
 * All rights reserved.
 *
 * This software is distributed under the terms of the LICENSE file
 * found in the root directory of this distribution
**********************************************************************/

#include <iostream>
#include <unistd.h>
#include <chrono>

#ifdef _WIN64
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
#endif

#define DEBUG_TIMESTEP 1 // seconds

enum MODE {CLIENT, SERVER};

void print_usage(const char* name)
{
    std::cout << "Usage: " << name << " [-cshd] [bind_addr] [bind_port] [remote_addr] [remote_port]" << std::endl;
    std::cout << "  -c: run as client" << std::endl;
    std::cout << "  -s: run as server" << std::endl;
    std::cout << "  -h: print this help" << std::endl;
    std::cout << "  -d: print debug info" << std::endl;
    std::cout << "Where: Server Mode" << std::endl;
    std::cout << "  bind_addr: address to bind to" << std::endl;
    std::cout << "  bind_port: port to bind to" << std::endl;
    std::cout << "  remote_addr: address to connect to when a client sends a datagram" << std::endl;
    std::cout << "  remote_port: port to connect to when a client sends a datagram" << std::endl;
    std::cout << "Where: Client Mode" << std::endl;
    std::cout << "  bind_addr: address to bind to" << std::endl;
    std::cout << "  bind_port: port to bind to" << std::endl;
    std::cout << "  remote_addr: The address of the udp obf running as server mode" << std::endl;
    std::cout << "  remote_port: The port of the udp obf running as server mode" << std::endl;
}

bool is_number(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int add_rotate(int b, int n)
{
    return (b + n) % 256;
}

void obfuscate_and_send_to(char* buffer, int len, int obf, int sock, struct sockaddr_in destination)
{
    // obfuscate data
    for (int i = 0; i < len; i++)
    {
        buffer[i] = add_rotate(buffer[i], obf);
    }

    // send data to remote
    if (sendto(sock, buffer, len, 0, (struct sockaddr*)&destination, sizeof(destination)) < 0)
    {
        std::cout << "\33[2K\rWarning: failed to send data to remote" << std::endl; 
        return;
    }
}

void handle_sigint(int s)
{
    std::cout << std::endl << "Exiting..." << std::endl;

    // Handle windows cleanup
    #ifdef _WIN64
        WSACleanup();
    #endif

    exit(0);
}

void print_bps(int data_lensum)
{
    std::string unit = "B";
    std::string unit2 = "b";
    float data_len = data_lensum;
    if (data_len > 1024)
    {
        data_len /= 1024;
        unit = "KiB";
        unit2 = "Kb";
    }
    if (data_len > 1024)
    {
        data_len /= 1024;
        unit = "MiB";
        unit2 = "Mb";
    }
    if (data_len > 1024)
    {
        data_len /= 1024;
        unit = "GiB";
        unit2 = "Gb";
    }
    std::cout << "\33[2K\rDEBUG: " << data_len << " " << unit << "/s" << " [" << data_len * 8 << " " << unit2 << "/s]" << std::flush;
}

void forward(std::string s_bind_addr, int bind_port, std::string s_remote_addr, int remote_port, int obf, bool debug)
{
    // create a socket and listen on udp
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        std::cout << "FATAL: failed to create socket" << std::endl;
        return;
    }

    // bind to the address and port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = inet_addr(s_bind_addr.c_str());

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        std::cout << "FATAL: failed to bind to " << s_bind_addr << ":" << bind_port << std::endl;
        return;
    }

    if (debug)
        std::cout << "DEBUG: Socket bound to " << s_bind_addr << ":" << bind_port << std::endl;

    // create a buffer to store the data
    char buffer[65536];

    // create a remote address to send to
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    remote_addr.sin_addr.s_addr = inet_addr(s_remote_addr.c_str());

    struct sockaddr_in last_addr;

    auto start_time = std::chrono::high_resolution_clock::now();
    int data_lensum = 0;

    // loop forever
    while (true)
    {
        struct sockaddr_in sender_addr;

        // Fuck you windows
        #ifdef _WIN64
            int sender_addr_len = sizeof(sender_addr);
        #else
            socklen_t sender_addr_len = sizeof(sender_addr);
        #endif

        // receive data
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
        if (len < 0)
        {
            std::cout << "\33[2K\rWarning: failed to receive data" << std::endl;
            return;
        }
        if (debug) {
            data_lensum += len;

            // check if it has been 5 seconds from start_time
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
            if (duration.count() >= DEBUG_TIMESTEP)
            {
                print_bps(data_lensum / DEBUG_TIMESTEP);
                start_time = std::chrono::high_resolution_clock::now();
                data_lensum = 0;
            }
        }

        // check if sender is remote_addr
        if (sender_addr.sin_addr.s_addr == remote_addr.sin_addr.s_addr && sender_addr.sin_port == remote_addr.sin_port)
        {
            obfuscate_and_send_to(buffer, len, obf, sock, last_addr);
            continue;
        } else {
            last_addr = sender_addr;
        }

        // deobf and send 
        obfuscate_and_send_to(buffer, len, -obf, sock, remote_addr);
    }
}

int main(int argc, char** argv)
{
    MODE mode = CLIENT;

    bool debug = false;
    std::string bind_addr, remote_addr;
    int bind_port, remote_port;
    int c;

    while ((c = getopt(argc, argv, "cshd")) != -1)
    {
        switch(c)
        {
            case 'c':
                mode = CLIENT;
                break;
            case 's':
                mode = SERVER;
                break;
            case 'd':
                debug = true;
                continue;
            case 'h':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (argc - optind < 4)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string bind_port_str, remote_port_str;

    bind_addr = argv[optind++];

    if (!is_number((bind_port_str = argv[optind++])))
    {
        std::cout << "FATAL: bind port must be a number" << std::endl;
        return 1;
    }

    remote_addr = argv[optind++];
    
    if (!is_number((remote_port_str = argv[optind++])))
    {
        std::cout << "FATAL: remote port must be a number" << std::endl;
        return 1;
    }

    bind_port = std::stoi(bind_port_str);
    remote_port = std::stoi(remote_port_str);
    
    if (bind_port < 0 || bind_port > 65535)
    {
        std::cout << "FATAL: bind port must be between 0 and 65535" << std::endl;
        return 1;
    }

    if (remote_port < 0 || remote_port > 65535)
    {
        std::cout << "FATAL: remote port must be between 0 and 65535" << std::endl;
        return 1;
    }

    // handle sigint
    signal(SIGINT, handle_sigint);

    // I dont like windows 
    #ifdef _WIN64
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (debug)
            std::cout << "DEBUG: WSAStartup" << std::endl;
    #endif

    switch (mode) {
        case CLIENT:
            if (debug)
                std::cout << "DEBUG: Starting from CLIENT mode" << std::endl;
            forward(bind_addr, bind_port, remote_addr, remote_port, -1, debug);
            break;
        case SERVER:
            if (debug)
                std::cout << "DEBUG: Starting from SERVER mode" << std::endl;
            forward(bind_addr, bind_port, remote_addr, remote_port, 1, debug);
            break;
    }
    
}
