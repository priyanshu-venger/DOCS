/**
 * @file server.cpp
 * @brief A simple multi-threaded server that processes client requests using the RESP (REdis Serialization Protocol).
 * The server handles SET and GET commands, stores data, and uses `epoll` for scalable I/O handling.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include "REPL.h"

/// Atomic flag used for synchronization between threads
atomic_flag flag = ATOMIC_FLAG_INIT;

/// Maximum number of events that can be handled at once by epoll
#define MAX_EVENTS 1024

/// Buffer size for receiving data from the client
#define BUFFER_SIZE 4096

/// Port number on which the server listens
#define PORT 6379

/// Global REPL object for storing key-value pairs in memory
REPL r;  

/**
 * @struct ClientContext
 * @brief Maintains the context and state of each connected client.
 *
 * This structure is used to track the client's socket file descriptor (`fd`), the buffer of data being parsed,
 * and the parsing state. It also tracks the arguments of the current command being processed.
 */
struct ClientContext {
    int fd; ///< File descriptor for the client connection
    std::string buffer; ///< Data buffer for storing incoming data from the client
    std::vector<std::string> args; ///< List of arguments for the current command
    size_t expected_bulk_length; ///< Expected length of the next bulk string argument
    int expected_args; ///< Number of arguments expected for the current command
    enum { PARSE_TYPE, PARSE_ARGUMENTS } state; ///< Current state of the parsing process
    
    /**
     * @brief Constructor to initialize the client context with a file descriptor
     * @param fd_ The file descriptor for the client connection
     */
    ClientContext(int fd_) : fd(fd_), expected_bulk_length(0), expected_args(0), state(PARSE_TYPE) {}
};

/**
 * @class ThreadPool
 * @brief A simple thread pool to handle client connections concurrently.
 *
 * The thread pool processes tasks such as handling client requests, parsing commands, and interacting
 * with the REPL data store. Tasks are queued and executed by worker threads.
 */
class ThreadPool {
public:
    /**
     * @brief Constructs a thread pool with a specified number of threads.
     * @param numThreads The number of worker threads in the pool
     */
    ThreadPool(size_t numThreads);

    /**
     * @brief Destructor to join all threads and clean up resources.
     */
    ~ThreadPool();

    /**
     * @brief Enqueues a new task to be executed by a worker thread.
     * @param task A function to be executed by a worker thread
     */
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers; ///< Vector of worker threads
    std::queue<std::function<void()>> tasks; ///< Queue of tasks to be executed
    std::mutex queue_mutex; ///< Mutex for synchronizing access to the task queue
    std::condition_variable condition; ///< Condition variable to notify worker threads
    bool stop; ///< Flag indicating whether the pool should stop or continue processing tasks

    /**
     * @brief Worker function that processes tasks from the queue.
     */
    void worker();
};

// ThreadPool constructor implementation
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }
}

// ThreadPool destructor implementation
ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }
}

// Enqueue a new task in the thread pool
void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        tasks.push(task);
    }
    condition.notify_one();
}

// Worker thread function to process tasks
void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

/**
 * @brief Sets a socket to non-blocking mode.
 * @param fd The file descriptor of the socket
 */
void set_nonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

/**
 * @brief Sends a response to the client.
 * @param fd The file descriptor of the client socket
 * @param response The response message to send to the client
 */
void send_response(int fd, const std::string& response) {
    send(fd, response.c_str(), response.size(), 0);
}

/**
 * @brief Handles the client's requests, parsing commands and interacting with the REPL data store.
 * This function is responsible for parsing the RESP commands from the client, executing commands like "SET" and "GET",
 * and sending responses back to the client.
 * @param ctx The client context containing the state and data for the client
 */
void handle_client(ClientContext& ctx) {
    flag.clear(memory_order_release);
    while (true) {
        if (ctx.state == ClientContext::PARSE_TYPE) {
            if (ctx.buffer.empty()) {
                break;
            }
            char type = ctx.buffer[0];
            ctx.buffer.erase(0, 1);
            if (type == '*') {
                size_t pos = ctx.buffer.find("\r\n");
                if (pos == std::string::npos) {
                    break;
                }
                ctx.expected_args = std::stoi(ctx.buffer.substr(0, pos));
                ctx.buffer.erase(0, pos + 2);
                ctx.args.clear();
                ctx.state = ClientContext::PARSE_ARGUMENTS;
            } else {
                send_response(ctx.fd, "-ERR Unsupported command\r\n");
                ctx.buffer.clear();
                break;
            }
        }
        while (ctx.state == ClientContext::PARSE_ARGUMENTS) {
            if (ctx.expected_args == 0) {
                if (ctx.args.empty()) {
                    send_response(ctx.fd, "-ERR Empty command\r\n");
                } else {
                    std::string command = ctx.args[0];
                    if (command == "SET" && ctx.args.size() == 3) {
                        if (r.SET(ctx.args[1], ctx.args[2])) send_response(ctx.fd, "+OK\r\n");
                        else send_response(ctx.fd, "-ERR\r\n");
                    } else if (command == "GET" && ctx.args.size() == 2) {
                        std::string value;
                        auto res = r.GET(ctx.args[1], value);
                        if (res) {
                            std::string response = "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
                            send_response(ctx.fd, response);
                        } else {
                            send_response(ctx.fd, "$-1\r\n");
                        }
                    } else {
                        send_response(ctx.fd, "-ERR Unknown command\r\n");
                    }
                }
                ctx.state = ClientContext::PARSE_TYPE;
                break;
            }
            if (ctx.buffer.size() < 4) {
                break;
            }
            if (ctx.buffer[0] != '$') {
                send_response(ctx.fd, "-ERR Protocol error\r\n");
                ctx.buffer.clear();
                ctx.state = ClientContext::PARSE_TYPE;
                break;
            }
            size_t pos = ctx.buffer.find("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            ctx.expected_bulk_length = std::stoi(ctx.buffer.substr(1, pos - 1));
            ctx.buffer.erase(0, pos + 2);
            if (ctx.buffer.size() < ctx.expected_bulk_length + 2) {
                break;
            }
            std::string arg = ctx.buffer.substr(0, ctx.expected_bulk_length);
            ctx.args.push_back(arg);
            ctx.buffer.erase(0, ctx.expected_bulk_length + 2);
            ctx.expected_args--;
        }
    }
}

/**
 * @brief Signal handler to cleanly shut down the server on SIGINT (Ctrl+C).
 * This function cleans up resources when the server is interrupted and exits the program.
 * @param sig The signal number (ignored in this implementation)
 */
void SIGHANDLER(int sig) {
    r.~REPL();
    exit(0);
}

/**
 * @brief The main entry point for the server application.
 * This function sets up the server socket, listens for incoming client connections,
 * and uses `epoll` to handle multiple clients concurrently. It also processes client commands
 * using the thread pool for parallel execution.
 * @return 0 on success, -1 on failure
 */
int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    signal(SIGINT, SIGHANDLER);
    
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("Listen failed");
        close(listen_fd);
        return -1;
    }

    set_nonblocking(listen_fd);

    int epoll_fd = epoll_create1(0);
    struct epoll_event event;
    event.data.fd = listen_fd;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

    std::unordered_map<int, ClientContext> clients;

    ThreadPool thread_pool(4);  // 4 threads in the pool

    struct epoll_event events[MAX_EVENTS];
    while (true) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                int client_fd = accept(listen_fd, nullptr, nullptr);
                if (client_fd == -1) {
                    perror("Accept failed");
                    continue;
                }
                set_nonblocking(client_fd);
                event.data.fd = client_fd;
                event.events = EPOLLIN | EPOLLET;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                clients.emplace(client_fd, ClientContext(client_fd));
            } else {
                auto it = clients.find(fd);
                if (it != clients.end()) {
                    ClientContext& ctx = it->second;
                    char buffer[BUFFER_SIZE];
                    ssize_t count;
                    while ((count = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
                        ctx.buffer.append(buffer, count);
                    }
                    thread_pool.enqueue([&ctx] { handle_client(ctx); });
                    while(flag.test_and_set(memory_order_acquire));
                    if (count == 0 || (count == -1 && errno != EAGAIN)) {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        clients.erase(it);
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}