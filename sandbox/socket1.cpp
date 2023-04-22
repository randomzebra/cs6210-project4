#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

#define PORT 8080

int main(int argc, char **argv) {
    int server_fd, valread, incoming_socket;
    int opt = 1;
    int count = 0;
    
    char* ack = "Hello from server";
    char buffer[1024] = { 0 };
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "SERVER: Socket creation error" << std::endl;
        return  1;
    }
    std::cerr << "SERVER FD: " << server_fd << std::endl;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "SERVER: Socket Option failure with error code " << errno << std::endl;
        return 1;
    }

    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
        std::cerr << "SERVER: Bind failed with error code " << errno << std::endl;
        return 1;
    }

    while (count < 5) {
        std::cout << "SERVER: On message " << ++count << std::endl;
        if (listen(server_fd, 5) < 0) {
            std::cerr << "SERVER: Listen failed with error code " << errno << std::endl;
            return 1;
        }
        std::cout << "SERVER: Listening..." << std::endl;
        if ((incoming_socket = accept(server_fd, (struct sockaddr *)&addr, (socklen_t *) &addr_len)) < 0) {
            std::cerr << "SERVER: Accept failed with error code " << errno << std::endl;
            return 1;
        }

        valread = read(incoming_socket, buffer, 1024);
        std::cout << "SERVER RECEIVED: " << buffer << std::endl;
        send(incoming_socket, ack, strlen(ack), 0);
        std::cout << "SERVER: SENDING ACK" << std::endl;
        close(incoming_socket);
    }
    



    
    shutdown(server_fd, SHUT_RDWR);
    return 0;
    

}
