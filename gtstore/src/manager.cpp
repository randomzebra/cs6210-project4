#include "gtstore.hpp"
#include <string.h>

void GTStoreManager::init(int nodes, int k) {
	this->total_nodes = nodes;
	this->k = k;
	live = true; //This should be false when GTStoreManager should die gracefully

	if (socket_init() < 0) {
		std::cerr << "MANAGER: socket_init() failed" << std::endl;
		return;
	}
	
	if (node_init() < 0) {
		std::cerr << "MANAGER: node_init() failed" << std::endl;
		return;
	}
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
			this->uninitialized.push_back(msg->node);
		}

		if (send(incoming, "Discover ack", strlen("Discover ack"), 0) < 0) {
			perror("MANAGER: send discover ack back to node");
			return -1;
		}
		close(incoming);
		counter++;
	}

	// std::cout << "discovered nodes: {";
	// for (auto it = this->uninitialized.begin(); it != this->uninitialized.end(); ++it) {
	// 	 std::cout << " ";
	// 	 print_node(*it);
	// }
	// std::cout << "}\n";



	vector<std::shared_ptr<store_grp_t>> groups;
	while(uninitialized.size() > 0) {
		auto group = std::make_shared<store_grp_t>();
		group->primary = uninitialized.back();
		uninitialized.pop_back();
		for (int i = 0; i < k - 1 && uninitialized.size() > 0; ++i) {
			group->num_neighbors++;
			group->neighbors[i] = uninitialized.back();
			uninitialized.pop_back();
		}
		groups.push_back(group);
	}

	for (auto group : groups) {
		rr.push(group);
		// print_group(*group);
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
void GTStoreManager::push_group_assignments(vector<std::shared_ptr<store_grp_t>> groups) {
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

 		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in servaddr = group->primary.addr;

		// Connect to the server
		if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
			perror("SERVER: connect to storage node during init");
			return;
		}
		
		assignment_message msg{*group};

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
	char* buffer = (char*)malloc(BUFFER_SZE * sizeof(char));
	if (!buffer) {
		perror("listen_for_msgs, buffer malloc");
		return -1;
	}

	// Get the socket address and port number
	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	if (getsockname(this->listen_fd, (struct sockaddr *)&sin, &sinlen) == -1) {
		perror("getsockname");
		return -1; 
	}
	// std::cout << "socket address: " << inet_ntoa(sin.sin_addr) << " port: " << ntohs(sin.sin_port) << "\n";

	// Listen for incoming connections
	if (listen(this->listen_fd, SOMAXCONN) == -1) {
		perror("listen");
		return -1;
	}

	while(true) {
		// Accept incoming connections
		int client_fd = accept(this->listen_fd, (struct sockaddr *)&sin, &sinlen);
		// std::cout << "[MANAGER [accept]] socket address: " << inet_ntoa(sin.sin_addr) << " port: " << ntohs(sin.sin_port) << "\n";
		if (client_fd < 0) {
			perror("MANAGER: accept");
			continue;
		}

		// Read data from the client
		if (read(client_fd, buffer, BUFFER_SZE) < 0) {
			perror("MANAGER: read");
			close(client_fd);
			continue;
		}

		// std::cout << "MANAGER: received message: " << buffer << "\n";

		comDemux(buffer, &sin, client_fd);
		close(client_fd);
	}
}

void GTStoreManager::comDemux(char* buffer, sockaddr_in* sin, int client_fd) {
	auto type = ((generic_message *)buffer)->type;

	switch (type) {
	case PUT:
		{
			auto msg = (comm_message*)buffer;
			// std::cout << "[MANAGER] PUT (key=" << msg->key << ",value=" << msg->value << ")\n";
			auto group = put(msg->key, msg->value);
			if (group == nullptr) {
				std::cout << "MANAGER: no group found for put!";
				return;
			}
			// print_group(*group);
			//std::cout << "[MANAGER [demux]] returning packet to: " << inet_ntoa(sin->sin_addr) << " port: " << ntohs(sin->sin_port) << "\n";

			assignment_message outgoing_msg{*group};

			if (send(client_fd, (void*)&outgoing_msg, sizeof(outgoing_msg), 0) == -1) {
				perror("MANAGER: (comdemux) send group msg");
				return;
			};
		}
		break;
	case GET:
		// std::cout << "[MANAGER] recieved get\n";
		{
			auto msg = (comm_message*)buffer;
			// std::cout << "[MANAGER] GET (key=" << msg->key << "\n";
			auto search = key_group_map.find(msg->key);
	
			assignment_message outgoing_msg{};
			if (search == key_group_map.end()) { 
				// std::cout << "MANAGER: no group found for get!";
				outgoing_msg.type = FAIL;
				return;
			} else {
				outgoing_msg.type = S_INIT;
				outgoing_msg.group = *search->second;
			}

			if (send(client_fd, (void*)&outgoing_msg, sizeof(outgoing_msg), 0) == -1) {
				perror("MANAGER: (comdemux) send group msg");
				return;
			};
		}
		break;
		//comm_message *msg = (comm_message *) buffer;
	case ACKPUT:
		// std::cout << "[MANAGER] recieved ackput, TODO\n";
		break;
	case NODE_FAILURE: {
		// std::cout << "[MANAGER] a node has failed!\n";
		auto msg = (node_failure_message*)buffer;
		if (handle_node_failure(msg->node) != 0) {
			std::cerr << "[MANAGER] unable to handle node failure\n";
			// send NACK
		} else {
			// send ACK
		}
		break;
	}
	default:
		std::cout << "[MANAGER] unknown msg type! type=" << type << "\n";
	}
}

std::shared_ptr<store_grp_t> GTStoreManager::put(std::string key, val_t val) {
	auto search = key_group_map.find(key);
	
	if (search == key_group_map.end()) { //No existing put found, allocate existing grp w/ RR
		if (rr.size() == 0) {
			std::cout << "MANAGER: no groups available in RR!";
			return nullptr; //Failure, if there are no allocated nodes and no rr groups, we have zero nodes
		}

		auto group = rr.front();
		rr.pop();
		rr.push(group);

		key_group_map.insert({key, group});

		//std::cerr << "MANAGER: group to return ";
		//print_group(*group);
		return group;
	} else {
		return search->second; //else return the found grp.
	}
}

int GTStoreManager::commit_push(string key, store_grp_t * strgrp) {
	return -1;
}

int GTStoreManager::handle_node_failure(node_t node) {
	auto search = node_keys_map.find(node);
	
	if (search == node_keys_map.end()) {
		std::cerr << "[MANAGER] node_failure: node (";
		// print_node(node);
		std::cerr << ") DNE in node_keys_map\n";
		return 0;
	}

	// loop through every key and remove node from group
	for (const auto& key : search->second) {
		auto grp_search = key_group_map.find(key);
		
		if (grp_search == key_group_map.end()) {
			std::cerr << "[MANAGER] node_failure: key (" << key << ") DNE in key_group_map\n";
			continue;
		}

		auto group = grp_search->second;
		if (group->num_neighbors == 0) {
			std::cerr << "[MANAGER] node_failure: num neighbors = 0, no alternatives!\n";
			group->primary.alive = false;
			return -1;
		}

		std::cerr << "[MANAGER] node_failure: prev group";
		// print_group(*group);

		if (group->primary.addr.sin_port == node.addr.sin_port) { // TODO: better equity
			// assign first neighbor as primary
			auto replacement = group->neighbors[0];
			group->primary = replacement;
			group->neighbors[0].alive = false;
			group->num_neighbors--;
		} else {
			for (auto i=0; i < group->num_neighbors; ++i) {
				if (group->neighbors[i].addr.sin_port == node.addr.sin_port) {
					group->neighbors[i].alive = false;
				}
			}

		}
		std::cerr << "[MANAGER] node_failure: new group";
		// print_group(*group);
	}

	return 0;
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
