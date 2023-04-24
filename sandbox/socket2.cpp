#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <iostream>

#define PORT 8080
  
int main(int argc, char const* argv[])
{
    int status, valread, client_listen, client_send, incoming;
    uint32_t port;
    struct sockaddr_in client_addr;
    char* hello = "Hello from client";
    int opt = 1;
    int addrlen = sizeof(client_addr);
    char buffer[1024] = { 0 };
    

    if ((client_send = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("CLIENT: Send socket failed");
        return -1;
    }

    if ((client_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("CLIENT: Listen socket error");
        return -1;
    }

    if (setsockopt(client_listen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Client: Option Selection Failed");
        return -1;
    }


    if (listen(client_listen, 3) < 0) {
        perror( "CLIENT: Listen failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    if (getsockname(client_listen, (struct sockaddr *)&sin, &sinlen) == -1)
        perror("getsockname");
    else
        port = ntohs(sin.sin_port);

    std::cout << "CLIENT: Port: " << port << std::endl;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status
         = connect(client_send, (struct sockaddr*)&server_addr,
                   sizeof(server_addr)))
        < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    

    send(client_send, &port, sizeof(port), 0);
    printf("Port message sent\n");
    valread = read(client_send, buffer, 1024);
    std::cout << "CLIENT: server ack " << buffer << std::endl;

    // Wait for dynamic response

    

    if ((incoming
         = accept(client_listen, (struct sockaddr*)&sin,
                  &sinlen))
        < 0) {
        perror("CLIENT: Accept failed");
        exit(EXIT_FAILURE);
    }

    valread = read(incoming, buffer, 1024);
    std::cout << "CLIENT RECEIVED: " << buffer << std::endl;
    send(incoming, "client ack", strlen("client ack"), 0);
    close(incoming);
    close(client_send);
    shutdown(client_listen, SHUT_RDWR);
    return 0;
}