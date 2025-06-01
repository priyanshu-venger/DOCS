/**
 * @file tcp_server.cpp
 * @brief This is a simple TCP server that listens for client connections and responds with the number of bytes received.
 * 
 * The server listens on a specified port, accepts incoming client connections,
 * receives data, and sends an acknowledgment with the number of bytes received.
 */



#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <ctime>

#define PORT 12345 ///< Port number for the connection
#define BUFFER_SIZE 2048 ///< Size of the buffer used for communication
#define LOG_FILE "1M.log"




/**
 * @brief Get the CPU usage of a specific process.
 * 
 * This function calculates the CPU usage percentage of a given process by reading 
 * its `/proc/<pid>/stat` file and the system-wide CPU usage from `/proc/stat`. 
 * It compares the CPU times over a 1-second interval to compute the usage.
 * 
 * @param pid The process ID of the target process.
 * @return The CPU usage percentage of the process.
 */
float get_cpu_usage(int pid) {
    // Read the previous CPU time and total CPU time from /proc/stat
    std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
    if (!stat_file.is_open()) {
        std::cerr << "Error opening /proc/" + std::to_string(pid) + "/stat" << std::endl;
        return 0.0f;
    }

    long utime, stime, cutime, cstime, vsize, rss;
    stat_file >> pid >> utime >> stime >> cutime >> cstime >> vsize >> rss;
    stat_file.close();

    long total_process_time = utime + stime + cutime + cstime;

    // Get the total CPU time from /proc/stat
    std::ifstream cpu_stat_file("/proc/stat");
    std::string cpu_line;
    std::getline(cpu_stat_file, cpu_line);
    std::istringstream cpu_stream(cpu_line);
    
    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    cpu_stream >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    cpu_stat_file.close();

    long total_cpu_time = user + nice + system + idle + iowait + irq + softirq + steal;

    // Calculate CPU usage percentage
    static long prev_total_process_time = 0;
    static long prev_total_cpu_time = 0;

    long delta_process_time = total_process_time - prev_total_process_time;
    long delta_cpu_time = total_cpu_time - prev_total_cpu_time;

    prev_total_process_time = total_process_time;
    prev_total_cpu_time = total_cpu_time;

    if (delta_cpu_time == 0) return 0.0f;

    // Return the percentage of CPU used by the process
    return (delta_process_time * 100.0f) / delta_cpu_time;
}




/**
 * @brief Get the memory usage of a specific process.
 * 
 * This function retrieves the memory usage of a process by reading the `/proc/<pid>/status` file.
 * The memory usage is returned in kilobytes (KB).
 * 
 * @param pid The process ID of the target process.
 * @return The memory usage of the process in kilobytes (KB).
 */
long get_memory_usage(int pid) {
    // Read memory usage from /proc/<pid>/status
    std::ifstream status_file("/proc/" + std::to_string(pid) + "/status");
    if (!status_file.is_open()) {
        std::cerr << "Error opening /proc/" + std::to_string(pid) + "/status" << std::endl;
        return 0;
    }

    std::string line;
    long rss = 0;
    while (std::getline(status_file, line)) {
        if (line.find("VmRSS") != std::string::npos) {
            std::istringstream ss(line);
            std::string label;
            ss >> label >> rss;  // Get the memory usage (VmRSS in KB)
            break;
        }
    }

    status_file.close();
    return rss;
}




/**
 * @brief Monitor the CPU and memory usage of a process every minute.
 * 
 * This function continuously monitors the CPU and memory usage of the specified process
 * every minute. The data is logged to a CSV file, and printed to the console.
 * 
 * @param pid The process ID of the target process.
 */
void monitor_resource_usage(int pid) {
    std::ofstream log_file(LOG_FILE, std::ios::app);  // Open log file in append mode
    if (!log_file.is_open()) {
        std::cerr << "Error opening log file: " << LOG_FILE << std::endl;
        return;
    }

    // Log the header if it's the first entry
    log_file << "Timestamp,CPU_Usage(%),Memory_Usage(KB)" << std::endl;

    float cpu_usage;
    long memory_usage;
    std::time_t timestamp;
    while (true) {
        // Get CPU and memory usage
        cpu_usage = get_cpu_usage(pid);
        memory_usage = get_memory_usage(pid);

        // Get the current timestamp
        timestamp = std::time(nullptr);
        std::string time_str = std::ctime(&timestamp);
        time_str = time_str.substr(0, time_str.length() - 1);  // Remove newline character

        // Log the data to the file every minute
        log_file << time_str << "," << cpu_usage << "," << memory_usage << std::endl;

        // Output the resource usage to console (optional)
        std::cout << "CPU Usage: " << cpu_usage << "%, Memory Usage: " << memory_usage << " KB" << std::endl;

        // Sleep for 60 seconds before logging again
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }

    log_file.close();
}



/**
 * @brief Main function of the TCP server
 * 
 * This function creates a server socket, binds it to a specified port, listens
 * for incoming connections, and sends the received byte count back to the client.
 * 
 * @return Exit status (0 for success)
 */


int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Server: Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options (optional)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("Server: Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Bind to all interfaces
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Server: Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for clients
    if (listen(server_fd, 3) < 0) {
        perror("Server: Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // Get the PID of the current server process
    int pid = getpid();
    std::cout << "Server Process ID: " << pid << std::endl;

    // Start the monitoring thread
    std::thread monitor_thread(monitor_resource_usage, pid);

    while (true) {
        // Accept an incoming connection
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("Server: Accept failed");
            continue;
        }

        std::cout << "Server: Connection established" << std::endl;

        // Receive data from the client and send acknowledgment for each packet
        while (true) {
            ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                if (bytes_received == 0)
                    std::cout << "Server: Connection closed by client." << std::endl;
                else
                    perror("Server: Receive failed");
                break;
            }

            // Send acknowledgment (just the received size in this case)
            std::string acknowledgment = "ACK " + std::to_string(bytes_received) + " bytes";
            send(client_sock, acknowledgment.c_str(), acknowledgment.length(), 0);

            std::cout << "Server: Received " << bytes_received << " bytes, sent acknowledgment." << std::endl;
        }

        // Close the client socket after processing the request
        close(client_sock);
    }

    // Close the server socket (unreachable code in this loop)
    close(server_fd);

    // Join the monitoring thread (unreachable in this infinite loop)
    monitor_thread.join();

    return 0;
}
