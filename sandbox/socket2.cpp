#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

#define PORT 8080

int main(int argc, char **argv) {
    int status, ack, client_fd;
    struct sockaddr_in serv_addr;
    char* hello = "Hello from client";
    char buffer[1024] = { 0 };
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        std::cerr << "SERVER: Socket RCVTIMEO failure with error code " << errno << std::endl;
        return 1;
    }

    if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        std::cerr << "SERVER: Socket SNDTIMEO failure with error code " << errno << std::endl;
        return 1;
    }
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    if((status = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        std::cout << "CLIENT: connect failed with error number " << errno << std::endl;
        return -1;
    }
    std::cout << "CLIENT: status " << status << std::endl;
    

    send(client_fd, hello, strlen(hello), 0);
    std::cout << "CLIENT: sent message" << std::endl;

    ack = read(client_fd, buffer, 1024);
    std::cout << "CLIENT RECEIVED: " << buffer << std::endl;
    close(client_fd);
    return 0;
}
