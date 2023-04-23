#include "gtstore.hpp"
#include <string.h>

void GTStoreManager::init(int nodes, int k) {
	
	cout << "Inside GTStoreManager::init()\n";
	this->total_nodes = nodes;
	this->k = k;
	live = true; //This should be false when GTStoreManager should die gracefully
	
	if (socket_init() < 0) {
		std::cerr << "MANAGER: socket_init() failed" << std::endl;
		return;
	}
	
	//Node Discovery
	if (node_init() < 0) {
		std::cerr << "MANAGER: node_init() failed" << std::endl;
		return;
	}
	//TODO: socket code here
	
}

/*
* Sets up listen and connection sockets. Hard binds manager socket to 8080 so it can be discovered by client and node. Does not begin any socket operations
*/
int GTStoreManager::socket_init() {
	if ((this->listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("MANAGER: listen socket");
		return -1;
	} 

	if ((this->connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("MANAGER: connect socket");
		return -1;
	}

	
    int opt = 1;
    //int addrlen = sizeof(listen_addr);
	listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(PORT);

	if (setsockopt(this->listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("MANAGER: Option Selection Failed");
        return -1;
    }

	if (bind(this->listen_fd, (struct sockaddr*) &listen_addr, sizeof(listen_addr)) < 0) {
        perror("MANAGER: Bind failed");
        return -1;
    }

	/*cant connect because we don't know where, but we can start to listen*/
	if (listen(this->listen_fd, 32) < 0) {
		perror("MANAGER: listen failed");
		return -1;
	}

	return 0;
}

/*
* Code for discovering storage nodes before operations can begin
*/
int GTStoreManager::node_init() {
	int incoming;

	
	int addrlen = sizeof(listen_addr);
	int counter = 0;
	char buffer[BUFFER_SZE] = {0};
	while (counter < total_nodes) {
		if ((incoming = accept(this->listen_fd, (struct sockaddr*) &listen_addr, (socklen_t *)&addrlen)) < 0) {
			perror("MANAGER: Node Init Accept failed");
        	return -1;
		}
		
		if (read(incoming, buffer, sizeof(buffer)) < 0) {
			perror("MANAGER: Node init Read failed");
			return -1;
		}


		discovery_message *msg = (discovery_message *) buffer;
		if (msg->type != DISC) {
			continue; //Drop any command messages at this stage
		} else {
			this->uninitialized.push_back(msg->discovery_port);
		}

		send(incoming, "Discover ack", strlen("Discover ack"), 0);
		close(incoming);
		counter++;
	}
	std::cout << "Discovered Nodes:";
	for (auto it = this->uninitialized.begin(); it != this->uninitialized.end(); ++it) {
		 std::cout << " " << *it;
	}
	std::cout << std::endl;
	return 0;
}

/*
* Receiving and processing commands. This is the main thread.
*/
int GTStoreManager::listen_for_coms() {
	int incoming;

	
	int addrlen = sizeof(listen_addr);
	int counter = 0;
	char buffer[BUFFER_SZE] = {0};


	while (true) {
		if ((incoming = accept(this->listen_fd, (struct sockaddr*) &listen_addr, (socklen_t *)&addrlen)) < 0) {
			perror("MANAGER: Node Init Accept failed");
        	return -1;
		}
		
		if (read(incoming, buffer, sizeof(buffer)) < 0) {
			perror("MANAGER: Node init Read failed");
			return -1;
		}

		if (((generic_message *) buffer)->type == PUT) {
			comm_message *put =(comm_message *) buffer;
			if (put_network(put->key, put->value) < 0) {
				put->type = FAIL;
				if (send(incoming, put, sizeof(&put), 0) < 0) {
					perror("MANAGER: Node init Read failed");
					return -1;
				}
				continue;
			}



		}
	}
}



int GTStoreManager::put_network(string key, val_t value) {

}

store_grp_t GTStoreManager::put(std::string key, val_t val) {
	if (uninitialized.size() > 0) {
		store_grp_t new_grp;
		for (size_t i = 0; i < uninitialized.size() && i < (unsigned) k; i++) {
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
