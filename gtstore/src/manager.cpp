#include "gtstore.hpp"
#include <string.h>
#include <sys/socket.h>

void GTStoreManager::init(int nodes, int k) {
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

	int res = bind(this->listen_fd, (struct sockaddr*) &listen_addr, (socklen_t)sizeof(listen_addr));
	if (res < 0) {
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

		if (send(incoming, "Discover ack", strlen("Discover ack"), 0) < 0) {
			perror("MANAGER: send discover ack back to node");
			return -1;
		}
		close(incoming);
		counter++;
	}

	std::cout << "discovered nodes: {";
	for (auto it = this->uninitialized.begin(); it != this->uninitialized.end(); ++it) {
		 std::cout << " " << *it;
	}
	std::cout << "}\n";



	vector<store_grp_t> groups;
	while(uninitialized.size() > 0) {
		store_grp_t group{};
		group.primary = uninitialized.back();
		uninitialized.pop_back();
		for (int i = 0; i < k - 1 && uninitialized.size() > 0; ++i) {
			group.num_neighbors++;
			group.neighbors[i] = uninitialized.back();
			uninitialized.pop_back();
		}
		groups.push_back(group);
	}

	for (auto& group : groups) {
		print_group(group);
	}
	
	push_group_assignments(groups);
	return 0;
}

int GTStoreManager::restart_connection(int mode) { //0 for no timeout, w/ 5 second timeout
	close(this->connect_fd);
	if ((this->connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("MANAGER: restart connection failed");
		return -1;
	}
	if (mode == 1) {
		struct timeval time_val_struct = { 0 };
		time_val_struct.tv_sec = 5;
		time_val_struct.tv_usec = 0;
		std::cout << "node init" << std::endl;
		if (setsockopt(this->connect_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
			perror("MANAGER: restart timeout opt failed");
			return -1;
		}

		if (setsockopt(this->connect_fd, SOL_SOCKET, SO_SNDTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
			perror("MANAGER: restart timeout opt failed");
			return -1;
		}	
		}
	
	return 0;
}

// tell a storage node that it's a primary and here are its children
void GTStoreManager::push_group_assignments(vector<store_grp_t> groups) {
	// called in node_init
	// needs to be called when it recieves a failure of a primary node, reassign primary node from source group
	//
	//
	// connect to storage node in `in_addr`, tell it you're the primary, give it group assignments
	//
	//
	// send message


	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;

	/*
	if (inet_pton(AF_INET, "127.0.0.1", &in_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return;
	}
	*/

	for (auto& group : groups) {
		// TODO: send packet to every storage node saying what group they're in
		// for now, only send to primary

 		int sockfd;
		struct sockaddr_in servaddr;

		// Create a socket
		sockfd = socket(AF_INET, SOCK_STREAM, 0);

		// Set up the server address and port number
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(group.primary);
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

		// Connect to the server
		if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
			perror("SERVER: connect to storage node during init");
			return;
		}
		
		assignment_message msg{};
		msg.type = S_INIT;
		msg.group = group;

		if (send(sockfd, (void*)&msg, sizeof(msg), 0) == -1) {
			perror("SERVER: send group msg");
			return;
		};

		// Close the socket
		close(sockfd);
		// TODO: listen for ACK?
	}
}

/*
* Receiving and processing commands. This is the main thread.
*/
int GTStoreManager::listen_for_msgs() {
	int incoming;

	
	int addrlen = sizeof(listen_addr);
	int counter = 0;
	
	

	while (true) {
		if ((incoming = accept(this->listen_fd, (struct sockaddr*) &listen_addr, (socklen_t *)&addrlen)) < 0) {
			perror("MANAGER: Node Init Accept failed");
        	return -1;
		}
		std::cout << "[listen_for_msgs] recived message!\n";
		//thread comms_routine (GTStoreManager::comDemux);
		comDemux();

	}
}

void GTStoreManager::comDemux() {
	char buffer[BUFFER_SZE] = {0};
	if (read(this->listen_fd, buffer, sizeof(buffer)) < 0) {
		perror("MANAGER: thread demux read failed");
		return;
	}
	std::cout << "[comDemux]: read message: '" << buffer << "'\n";

	if (((generic_message *) buffer)->type == PUT) {
		comm_message *msg = (comm_message *) buffer;
		

	} else if (((generic_message *) buffer)->type == GET) {
		comm_message *msg = (comm_message *) buffer;


	} else if (((generic_message *) buffer)->type == ACKPUT) {
		port_message *msg = (port_message *) buffer; // This will use the key field. Most of the time, we do not need it
	} else if (((generic_message *) buffer)->type == FAIL) {
		port_message *msg = (port_message *) buffer;
	} else {
		return; //Unrecognized message type
	}

}




store_grp_t GTStoreManager::put(std::string key, val_t val) {
	

	store_grp_t *existing_grp;
	auto record = existing_puts.find(key);
	
	if (record == existing_puts.end()) { //No existing put found, allocate existing grp w/ RR
		if (rr.size() == 0) {
			return {}; //Failure, if there are no allocated nodes and no rr groups, we have zero nodes
		}

		existing_grp = rr.front(); //RR
		rr.pop();
		rr.push(existing_grp);

		//existing_puts.insert(std::pair<std::string, store_grp_t *>(key, existing_grp)); //Performed after receivng an ack for the 
		return *existing_grp;
	} else {
		return *record->second; //else return the found grp.
	}
}

int GTStoreManager::commit_push(string key, store_grp_t * strgrp) {
	return -1;
}




int main(int argc, char **argv) {
	int n, k;
	// TODO: the test provides no arguments to this so...
	if (argc < 3) {
		n = 2;
		k = 2;
	} else {
		n = atoi(argv[1]);
		k = atoi(argv[2]);
	}

	GTStoreManager manager;
	manager.init(n, k);

	manager.listen_for_msgs();
}
