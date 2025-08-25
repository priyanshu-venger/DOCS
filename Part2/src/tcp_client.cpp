/**
 * @file tcp_client.cpp
 * @brief This is a simple TCP client that sends random data to the server and receives a response.
 * 
 * The client establishes a connection to the server, generates random data,
 * sends it to the server, and waits for an acknowledgment.
 */


#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <chrono>

#define SERVER_IP "127.0.0.1"  ///< IP address of the server
#define PORT 12345 ///< Port number for the connection
#define BUFFER_SIZE 2048 ///< Size of the buffer used for communication
int NUM_PACKETS=1;  



/**
 * @brief Main function of the TCP client
 * 
 * This function creates a socket, connects to the server, generates random data,
 * sends the data to the server, and receives the server's response.
 * 
 * @return Exit status (0 for success)
 */

int main(int argc, char * argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if(argc>1){
        NUM_PACKETS = atoi(argv[1]);
    }

    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Client: Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Define server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Client: Invalid address/Address not supported" << std::endl;
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Client: Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Measure start time
    auto start_time = std::chrono::high_resolution_clock::now();

    // Send 10,000 packets
    int total_data_sent = 0;
    for (int i = 0; i < NUM_PACKETS; ++i) {
        // Generate random data between 512 bytes and 1KB
        int data_size = (std::rand() % 513) + 512; // Random size between 512 and 1024
        char *data = new char[data_size];

        // Fill data with random bytes
        for (int j = 0; j < data_size; ++j) {
            data[j] = static_cast<char>(std::rand() % 256);
        }

        // Send data to server
        send(sock, data, data_size, 0);
        total_data_sent += data_size;

        // Receive acknowledgment from the server
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received string
            /* uncomment the line below to check the recieved packet by client */
            std::cout << "Client: Acknowledgment received for packet " << i + 1 << std::endl;
        } else {
            perror("Client: Receive failed");
        }

        // Clean up
        delete[] data;
    }

    // Measure end time
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_time = end_time - start_time;

    // Calculate latency (avg time per packet)
    double latency = elapsed_time.count() / NUM_PACKETS;

    // Calculate bandwidth (in MB/s)
    double bandwidth = (total_data_sent / 1024.0 / 1024.0) / elapsed_time.count(); // in MB/s

    // Print the metrics
    std::cout << "Client: Sent " << NUM_PACKETS << " packets (" << total_data_sent << " bytes)." << std::endl;
    std::cout << "Client: Latency: " << latency * 1000 << " ms per packet" << std::endl;  // in milliseconds
    std::cout << "Client: Bandwidth: " << bandwidth << " MB/s" << std::endl;

    // Close the socket
    close(sock);

    return 0;
}
