#include "network_channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <vector>

using namespace std;


/**
 * Creates a NetworkRequestChannel
 * 
 * @param ip IP address to connect to (client) or interface to bind to (server)
 *           Empty string for server side means bind to all interfaces
 * @param port Port number to use
 * @param side SERVER_SIDE to create a listening socket, CLIENT_SIDE to connect to a server
 * 
 * SERVER_SIDE behavior:
 * - Creates a socket and configures it for listening on the specified port
 * - Sets socket options to allow address reuse
 * 
 * CLIENT_SIDE behavior:
 * - Creates a socket and connects to the specified server address
 * - If the ip parameter is not a valid IP address, attempts to resolve it as a hostname
 * 
 * @throws Exits with error message if socket operations fail
 */

// Constructor for setting up a connection (server listening or client connecting)
NetworkRequestChannel::NetworkRequestChannel(const std::string& ip, int port, Side side) 
    : my_side(side), client_addr_len(sizeof(client_addr)) {
    
    // Initialize address structures to zero
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    
    if (side == SERVER_SIDE) {
        // TODO: Implement server-side socket creation and setup
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            cerr << "Error creating socket server side " << strerror(errno) << endl;
            throw("Error creating socket server side");
        } 
        int allow = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &allow, sizeof(allow)) == -1) {
            cerr << "Error allowing address reuse server side " << strerror(errno) << endl;
            throw("Error allowing address reuse server side");
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port); // CHECK THIS IF SOMETHING IS WRONG

        // Needed to convert ip string to machine usable ip
        if (ip.empty()) {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        }
        else {
            if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
                cerr << "Error setting address server side " << strerror(errno) << endl;
                throw("Error setting address server side");
            }
        }

        if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            cerr << "Error binding " << strerror(errno) << endl;
            throw("Error binding");
        }

        if (listen(sockfd, 10) == -1) {
            cerr << "Error listening " << strerror(errno) << endl;
            throw("Error listening");
        }

        // Set peer information for logging purposes
        peer_ip = "0.0.0.0";
        peer_port = port;
        cout << "Server listening on port " << port << endl;
    } else {
        // TODO: Implement client-side socket creation and connection
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            cerr << "Error creating socket client side " << strerror(errno) << endl;
            throw("Error creating socket client side");
        } 

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port); // CHECK THIS IF SOMETHING IS WRONG

        // Needed to convert ip string to machine usable ip
        if (ip == "localhost") {
            const char* new_ip = "127.0.0.1";
            if (inet_pton(AF_INET, new_ip, &server_addr.sin_addr) <= 0) {
                cerr << "Error setting address client side " << strerror(errno) << endl;
                throw("Error setting address client side");
            }
        }
        else {
            if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
                cerr << "Error setting address client side " << strerror(errno) << endl;
                throw("Error setting address client side");
            }
        }
        
        if (connect(sockfd, (const sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            cerr << "Error connecting " << strerror(errno) << endl;
            throw("Error connecting");
        }
        
        // Store peer information for logging
        peer_ip = ip;
        peer_port = port;
        cout << "Connected to server at " << ip << ":" << port << endl;
    }
}

/**
 * Constructor for client connections accepted by a server
 * 
 * @param fd Socket file descriptor returned by accept()
 * 
 * This constructor initializes a NetworkRequestChannel using an existing
 * socket connection that was established by accepting a client connection.
 */
NetworkRequestChannel::NetworkRequestChannel(int fd) 
    : my_side(SERVER_SIDE), sockfd(fd), client_addr_len(sizeof(client_addr)) {
    
    // TODO: Implement this constructor function
    if (getpeername(sockfd, (struct sockaddr*)&client_addr, &client_addr_len) == -1) {
        cerr << "Error getting peer info " << strerror(errno) << endl;
        throw("Error getting peer info");
    }

    // Store the IP address and port in peer_ip and peer_port
    char ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == NULL) {
        cerr << "Error getting ip " << strerror(errno) << endl;
        throw("Error getting ip");
    }
    peer_ip = std::string(ip);
    peer_port = ntohs(client_addr.sin_port);
}

/**
 * Destructor
 * 
 * Cleans up resources used by this NetworkRequestChannel
 */
NetworkRequestChannel::~NetworkRequestChannel() {
    // TODO: Implement the destructor

    if (close(sockfd) == -1) {
        cerr << "Error closing socket " << strerror(errno) << endl;
    }
    
}

/**
 * Accepts a new client connection on a server socket
 */
int NetworkRequestChannel::accept_connection() {
    // TODO: Accept a new client connection
    int new_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (new_sockfd == -1) {
        cerr << "Error accepting socket " << strerror(errno) << endl;
        return -1;
    }

    // Print connection information for logging purposes
    cout << "Accepted connection from " 
         << inet_ntoa(client_addr.sin_addr) << ":" 
         << ntohs(client_addr.sin_port) << endl;
    
    return new_sockfd; // Replace this with the actual new socket file descriptor
}

string NetworkRequestChannel::get_peer_address() const {
    return peer_ip + ":" + to_string(peer_port);
}

int NetworkRequestChannel::get_socket_fd() const {
    return sockfd;
}

/**
 * Sends a request to the server and waits for a response
 * 
 * @param req The Request object to send
 * @return Response from the server
 * 
 * This method:
 * Sends the length of the request followed by the request itself and receives the responses.
 * 
 * The wire format uses a 4-byte length header followed by the serialized data.
 * Request format: TYPE|USER_ID|AMOUNT|FILENAME|DATA
 * Response format: SUCCESS|BALANCE|DATA|MESSAGE
 * 
 * @throws May throw exceptions on network errors
 */
Response NetworkRequestChannel::send_request(const Request& req) {
    // Format: TYPE|USER_ID|AMOUNT|FILENAME|DATA
    stringstream ss;
    ss << static_cast<int>(req.type) << "|"
       << req.user_id << "|"
       << req.amount << "|"
       << req.filename << "|"
       << req.data;
    
    string request_str = ss.str();
    
    // Add message length as header (4 bytes)
    // Convert the request string length to network byte order
    uint32_t length = htonl(request_str.length());
    char length_buf[4];
    memcpy(length_buf, &length, 4);

    // TODO : Implement the send_request function
    size_t sent = 0;
    int i  = 0;
    // Send header
    while(sent < 4) {
        i = send(sockfd, length_buf + sent, 4 - sent, 0);
        if (i == -1) {
            perror("Send failed in send_request");
            return Response(false, 0, "", "Send failed");
        }
        sent += i;
    }

    char message_buf[1024];
    memcpy(message_buf, request_str.c_str(), request_str.size());
    
    sent = 0;
    
    while (sent < request_str.size()) { // Sending whole message
        i = send(sockfd, message_buf + sent, request_str.size() - sent, 0);
        if (i == -1) {
            perror("Send failed in send_request");
            return Response(false, 0, "", "Send failed");
        }
        sent += i;
    }

    // Read response header first
    size_t n = recv(sockfd, length_buf, 4, 0);
    if (n != 4) {
        perror("Receive failed in send_request");
        return Response(false, 0, "", "Receive failed");
    }

    uint32_t resp_converted;
    memcpy(&resp_converted, length_buf, 4);
    resp_converted = ntohl(resp_converted);

    char resp_buf[1024];
    size_t received = 0;
    i = 0;

    // Receiving whole message
    while (received < resp_converted) {
        i = recv(sockfd, resp_buf + received, resp_converted - received, 0);
        if (i == -1) {
            perror("Receive failed in send_request");
            return Response(false, 0, "", "Receive failed");
        }
        received += i;
    }

    resp_buf[resp_converted] = '\0';

    // Decode response
    string delimiter = "|";
    size_t pos = 0;
    bool success;
    double balance;
    std::string data;
    std::string message;
    string response_str(resp_buf);

    // Parse response using same delimiter format
    if ((pos = response_str.find(delimiter)) != string::npos) {
        success = (response_str.substr(0, pos) == "1");
        response_str.erase(0, pos + delimiter.length());
    }
    if ((pos = response_str.find(delimiter)) != string::npos) {
        balance = stod(response_str.substr(0, pos));
        response_str.erase(0, pos + delimiter.length());
    }
    if ((pos = response_str.find(delimiter)) != string::npos) {
        data = response_str.substr(0, pos);
        message = response_str.substr(pos + delimiter.length());
    }
    
    return Response(success, balance, data, message);
}

/**
 * Receives a request from a client
 * 
 * @return The received Request object
 * 
 * This method:
 * Receives the length of the incoming request (4-byte header) and the actual data
 * 
 */
Request NetworkRequestChannel::receive_request() {
    // TODO: Implement the receive_request function
    char req_buf[1024];
    int i;
    char length_buf[4];

    size_t n = recv(sockfd, length_buf, 4, 0);
    if (n != 4) {
        perror("Receive failed in receive_request");
        return Request(QUIT);
    }

    uint32_t req_converted;
    memcpy(&req_converted, length_buf, 4);
    req_converted = ntohl(req_converted);

    size_t received = 0;

    // Receiving whole message
    while (received < req_converted) {
        i = recv(sockfd, req_buf + received, req_converted - received, 0);
        if (i == -1) {
            perror("Receive failed in receive_request");
            return Request(QUIT);
        }
        received += i;
    }

    req_buf[req_converted] = '\0';
    

    return Request::parseRequest(req_buf);
}

/**
 * Sends a response to a client
 * 
 * @param resp The Response object to send
 * 
 * This method:
 * Sends the length of the response followed by the response itself
 * 
 * The wire format uses a 4-byte length header followed by the serialized data.
 * Response format: SUCCESS|BALANCE|DATA|MESSAGE
 */
void NetworkRequestChannel::send_response(const Response& resp) {
    // Format: SUCCESS|BALANCE|DATA|MESSAGE
    stringstream ss;
    ss << (resp.success ? "1" : "0") << "|"
       << resp.balance << "|"
       << resp.data << "|"
       << resp.message;
    
    string response_str = ss.str();
    
    uint32_t length = htonl(response_str.length());
    char length_buf[4];
    memcpy(length_buf, &length, 4);

    // TODO: Implement the send_response function
    size_t sent = 0;
    int i  = 0;
    // Send header
    while(sent < 4) {
        i = send(sockfd, length_buf + sent, 4 - sent, 0);
        if (i == -1) {
            perror("Send failed in send_response");
        }
        sent += i;
    }

    char message_buf[1024];
    memcpy(message_buf, response_str.c_str(), response_str.size());
    
    sent = 0;
    
    while (sent < response_str.size()) { // Sending whole message
        i = send(sockfd, message_buf + sent, response_str.size() - sent, 0);
        if (i == -1) {
            perror("Send failed in send_response");
        }
        sent += i;
    }
    
}