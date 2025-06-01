/**
 * @file client.cpp
 * @brief A simple client that communicates with a server using RESP (REdis Serialization Protocol).
 * This client sends commands to the server and processes the server's response.
 */

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096 ///< Buffer size for receiving data.
#define PORT 6379 ///< Port to connect to on the server.
#define SERVER_IP "127.0.0.1" ///< IP address of the server (127.0.0.1 for local).

/**
 * @brief Serializes a command into the RESP (REdis Serialization Protocol) format.
 *
 * The RESP format is used to encode commands that the server understands. It starts with a
 * number of parts in the command, followed by each part's size and content.
 *
 * @param cmd_parts A vector of strings, where each string is a part of the command.
 * @return A string representing the serialized RESP command.
 */
std::string serialize_command(const std::vector<std::string>& cmd_parts) {
    std::string resp = "*" + std::to_string(cmd_parts.size()) + "\r\n";
    for (const auto& part : cmd_parts) {
        resp += "$" + std::to_string(part.size()) + "\r\n" + part + "\r\n";
    }
    return resp;
}

/**
 * @brief Parses the response received from the server and prints it to the console.
 *
 * The response from the server is in the RESP format and can be a simple string, a bulk
 * string, or an error. This function interprets the response and prints it to the console.
 *
 * @param sock_fd The socket file descriptor used for communication with the server.
 */
void parse_response(int sock_fd) {
    char buffer[BUFFER_SIZE]; ///< Buffer to store the received response.
    ssize_t bytes_received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0); ///< Receive data.
    
    // If no data is received or an error occurs, print "(nil)"
    if (bytes_received <= 0) {
        std::cout << "(nil)" << std::endl;
        return;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the received string.
    std::string response(buffer);  // Convert the received buffer to a string.
    
    char type = response[0];  // The first character indicates the response type.
    
    if (type == '+') {
        // Simple string response
        std::cout << response.substr(1, response.find("\r\n") - 1) << std::endl;
    } else if (type == '$') {
        // Bulk string response
        if (response.substr(1, 2) == "-1") {
            std::cout << "(nil)" << std::endl;  // Null bulk string
        } else {
            size_t pos = response.find("\r\n");
            int length = std::stoi(response.substr(1, pos - 1));
            std::string data = response.substr(pos + 2, length);
            std::cout << data << std::endl;
        }
    } else if (type == '-') {
        // Error response
        std::cout << "(error) " << response.substr(1, response.find("\r\n") - 1) << std::endl;
    } else {
        // Unknown response type
        std::cout << "Unknown response" << std::endl;
    }
}

/**
 * @brief Splits an input line into individual command parts.
 *
 * The input line is expected to be a space-separated command, and this function splits it
 * into individual parts which will be used in the RESP serialization.
 *
 * @param line The input line from the user.
 * @return A vector of strings, where each string is a part of the command.
 */
std::vector<std::string> parse_input(const std::string& line) {
    std::vector<std::string> parts;  ///< Vector to store the command parts.
    std::istringstream iss(line);    ///< Input string stream to parse the line.
    std::string part;               ///< Temporary string to store each part of the command.
    
    while (iss >> part) {
        parts.push_back(part);  ///< Add each part to the vector.
    }
    return parts;
}

/**
 * @brief The main function that runs the client and communicates with the server.
 *
 * This function initializes the socket, connects to the server, and enters a loop where it
 * continuously reads input from the user, serializes the command, sends it to the server, and
 * parses the server's response.
 *
 * @return 0 on success, or -1 if an error occurs.
 */
int main() {
    // Create a socket for communication.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");  ///< Print error message if socket creation fails.
        return -1;
    }

    // Set up the server address and port.
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert the server IP address to binary format and check for errors.
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server IP address" << std::endl;
        close(sock_fd);
        return -1;
    }

    // Connect to the server and handle connection errors.
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(sock_fd);
        return -1;
    }

    std::string line;  ///< String to store user input.
    std::cout << "Connected to DOCS DB server." << std::endl;
    std::cout << "> ";  ///< Display prompt to the user.
    
    // Main loop to interact with the user.
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";  ///< If the input is empty, just prompt again.
            continue;
        }
        
        // Parse the user input and serialize the command.
        std::vector<std::string> cmd_parts = parse_input(line);
        std::string resp_cmd = serialize_command(cmd_parts);
        
        // Send the serialized command to the server.
        send(sock_fd, resp_cmd.c_str(), resp_cmd.size(), 0);
        
        // Parse and print the response from the server.
        parse_response(sock_fd);
        
        std::cout << "> ";  ///< Prompt for the next command.
    }

    // Close the socket and exit.
    close(sock_fd);
    return 0;
}
