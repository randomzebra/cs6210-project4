
// Server side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

#define PORT 8080
int main(int argc, char const* argv[])
{
    int server_send, server_listen, incoming, valread, status;
    uint32_t in_port;
    struct sockaddr_in server_addr;
    int opt = 1;
    int addrlen = sizeof(server_addr);
    char buffer[1024] = { 0 };
    char* hello = "Dynamic send!";

    //Socket setup

    if ((server_send = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SERVER: Send socket failed");
        return -1;
    }

    if ((server_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SERVER: Listen socket error");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (setsockopt(server_listen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("SERVER: Option Selection Failed");
        return -1;
    }

    if (bind(server_listen, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("SERVER: Bind failed");
        return -1;
    }

    if (listen(server_listen, 3) < 0) {
        perror( "SERVER: Listen failed");
        exit(EXIT_FAILURE);
    }

    if ((incoming
         = accept(server_listen, (struct sockaddr*)&server_addr,
                  (socklen_t*)&addrlen))
        < 0) {
        perror("SERVER: Accept failed");
        exit(EXIT_FAILURE);
    }

    valread = read(incoming, &in_port, sizeof(in_port));
    std::cout << "SERVER: port received " << in_port << std::endl;
    send(incoming, "ack", strlen("ack"), 0);
    close(incoming);

    struct sockaddr_in in_addr;


    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(in_port);

    if (inet_pton(AF_INET, "127.0.0.1", &in_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status
         = connect(server_send, (struct sockaddr*)&in_addr,
                   sizeof(in_addr)))
        < 0) {
        perror("Connection Failed");
        return -1;
    }

    send(server_send, hello, strlen(hello), 0);
    printf("Dynamic message sent\n");
    std::cout << "Sent: " << hello << strlen(hello) << std::endl;
    valread = read(server_send, buffer, 1024);
    std::cout << "SERVER: dynamic ack received " << buffer << std::endl;
    
    close(server_send);
    shutdown(server_listen, SHUT_RDWR);
    return 0;
}