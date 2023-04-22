#include "gtstore.hpp"
#include <netinet/in.h>
#include <string.h>

void GTStoreManager::init(int nodes, int k) {
	
	cout << "Inside GTStoreManager::init()\n";
	this->total_nodes = nodes;
	this->k = k;
	live = true; //This should be false when GTStoreManager should die gracefully
	//Node Discovery

	//TODO: socket code here
	listen_for_coms();
}


int GTStoreManager::node_init() {
	int server_fd, valread, incoming_socket;
    int opt = 1;
    int count = 0;
    
	std::cout << "Listening for nodes" << std::endl;
    char* ack = "Hello from server";
    char buffer[sizeof(int) * 4] = { 0 };
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "SERVER: Socket creation error" << std::endl;
        return 1;
    }
    std::cerr << "SERVER FD: " << server_fd << std::endl;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "SERVER: Socket Option failure with error code " << errno << std::endl;
        return 1;
    }

	struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        std::cerr << "SERVER: Socket RCVTIMEO failure with error code " << errno << std::endl;
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
	
	
	int count = 0;
    while (count < total_nodes) {
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

        valread = read(incoming_socket, buffer, BUFFER_SZE);
        std::cout << "SERVER RECEIVED: " << buffer << std::endl;
		int client_socket = *((int *) buffer);

        send(incoming_socket, ack, strlen(ack), 0);
        std::cout << "SERVER: SENDING ACK" << std::endl;
        close(incoming_socket);
    }
    



    
    shutdown(server_fd, SHUT_RDWR);
    return 0;
}

int GTStoreManager::listen_for_coms() {
	int server_fd, valread, incoming_socket;
    int opt = 1;
    int count = 0;
    

    char* ack = "Hello from server";
    char buffer[BUFFER_SZE] = { 0 };
    std::cout << "SERVER: Listening for commands" << std::endl;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "SERVER: Socket creation error" << std::endl;
        return 1;
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
	
	

    while (live) {
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

        valread = read(incoming_socket, buffer, BUFFER_SZE);
        std::cout << "SERVER RECEIVED: " << buffer << std::endl;
        send(incoming_socket, ack, strlen(ack), 0);
        std::cout << "SERVER: SENDING ACK" << std::endl;
        close(incoming_socket);
    }
    



    
    shutdown(server_fd, SHUT_RDWR);
    return 0;
    

}



store_grp_t GTStoreManager::put(std::string key, val_t val) {
	if (uninitialized.size() > 0) {
		store_grp_t new_grp;
		for (int i = 0; i < uninitialized.size() && i < k; i++) {
			new_grp.push_back(uninitialized.back());
			uninitialized.pop_back();
		}
		rr.push(&new_grp);
		return new_grp;
	} 

	store_grp_t *existing_grp;
	auto record = existing_puts.find(key);
	
	if (record == existing_puts.end()) { //No existing put found, allocate existing grp w/ RR
		if (rr.size() == 0) {
			return {}; //Failure, if there are no allocated nodes and no rr groups, we have zero nodes
		}

		existing_grp = rr.front(); //RR
		rr.pop();
		rr.push(existing_grp);

		existing_puts.insert(std::pair<std::string, store_grp_t *>(key, existing_grp)); //Add a new put entry
		return *existing_grp;
	} else {
		return *record->second; //else return the found grp.
	}
}




int main(int argc, char **argv) {

	GTStoreManager manager;
	manager.init(atoi(argv[1]), atoi(argv[2]));
	
}
