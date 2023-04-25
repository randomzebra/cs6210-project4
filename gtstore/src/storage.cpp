#include "gtstore.hpp"
#include <map>



void GTStoreStorage::init() {
	load = 0;
	state = UNINITIALIZED; //Leader is set upon first put to a node. Replica set by manager when another node is set as leader

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
* Attempts to discover an existing manager on PORT
*/
int GTStoreStorage::node_init() {
	struct timeval time_val_struct = { 0 };
	time_val_struct.tv_sec = 5;
	time_val_struct.tv_usec = 0;
	if (setsockopt(this->connect_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
		perror("STORAGE: Node_init timeout opt failed");
		return -1;
	}

	if (setsockopt(this->connect_fd, SOL_SOCKET, SO_SNDTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
		perror("STORAGE: Node_init timeout opt failed");
		return -1;
	}	

	//Set up a five second time out
	if (connect(this->connect_fd, (struct sockaddr*) &this->mang_connect_addr, sizeof(this->mang_connect_addr)) < 0) {
		perror("STORAGE: Node_init connect failed");
		return -1;
	}
	
	node_t self;
	self.addr = this->addr;
	self.alive = true;

	discovery_message msg;
	msg.type = DISC;
	msg.node = self;

	if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
		perror("STORAGE: Node_init send failed");
		return -1;
	}
	char buffer[BUFFER_SZE] = {0};
	read(this->connect_fd, buffer, sizeof(buffer));
	//std::cout << "STORAGE[" << listen_port << "]: Node init ack received '" << buffer << "'\n";
	close(this->connect_fd);

	//rePrep a socket and reset timeout to infinite

	if ((this->connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("STORAGE: connect socket");
		return -1;
	}
	time_val_struct.tv_sec = 0;
	time_val_struct.tv_usec = 0;
	if (setsockopt(this->connect_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
		perror("STORAGE: Node_init timeout opt failed");
		return -1;
	}

	if (setsockopt(this->connect_fd, SOL_SOCKET, SO_SNDTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
		perror("STORAGE: Node_init timeout opt failed");
		return -1;
	}	
	return 0;
}

int GTStoreStorage::listen_for_msgs() {
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
	//std::cout << "socket address: " << inet_ntoa(sin.sin_addr) << " port: " << ntohs(sin.sin_port) << "\n";

	// Listen for incoming connections
	if (listen(this->listen_fd, SOMAXCONN) == -1) {
		perror("listen");
		return -1;
	}

	while(true) {
		// Accept incoming connections
		int client_fd = accept(this->listen_fd, (struct sockaddr *)&sin, &sinlen);
		if (client_fd < 0) {
			perror("accept");
			continue;
		}

		// Read data from the client
		if (read(client_fd, buffer, BUFFER_SZE) < 0) {
			perror("read");
			close(client_fd);
			return -1;
		}

		if (com_demux(buffer, client_fd) != 0) {
			//close(client_fd);
			std::cerr << "STORAGE[" << addr.sin_port << "]: demux failed for " << sin.sin_port << "\n";
			//return -1;
		} else {
			//generic_message msg;
			//msg.type = ACK;
			//if (send(client_fd, &msg, sizeof(msg), 0) < 0) {
			//	perror("STORAGE: send ack failed after demux");
			//	return false;
			//} 

		}

		close(client_fd);
	}
}

// TODO: return packet on this func. ACK or NACK
int GTStoreStorage::com_demux(char* buffer, int client_fd) {
	generic_message *msg = (generic_message *) buffer;
	switch(msg->type) {
		case S_INIT: {
			// std::cout << "STORAGE[" << addr.sin_port << "]: init message recieved\n";
			assignment_message *msg = (assignment_message *) buffer;
			group = msg->group;
			return 0;
		}
		case PUT: {
			auto msg = (comm_message*)buffer;
			put(msg->key, msg->value);
			handle_put_msg(msg);


			comm_message resp;
			resp.type = ACKPUT;
			strcpy(resp.key, msg->key);
			strcpy(resp.value, msg->value);
			if (send(client_fd, &resp, sizeof(comm_message), 0) < 0) {
				perror("STORAGE: send ackput failed after demux");
				return false;
			} 

			return 0;
		}
		case GET: {
			// std::cout << "STORAGE[" << addr.sin_port << "]: get: key=(" << buffer << ")\n";

			auto msg = (comm_message*)buffer;
			comm_message resp;

			resp.type = ACKGET;
			strcpy(resp.key, msg->key);
			strcpy(resp.value, get(msg->key).c_str());
			if (send(client_fd, &resp, sizeof(comm_message), 0) < 0) {
				perror("STORAGE: send ackget failed after demux");
				return false;
			} 
		}
		case ACK:
			// std::cout << "STORAGE[" <<addr.sin_port << "]: ACK, ignoring\n";
			return 0;
		default:
			// std::cout << "STORAGE[" <<addr.sin_port << "]: unhandled message recieved type=(" << msg->type << ")\n";
			return -1;
	}
	return 0;
}

// send PUT to every neighbor
int GTStoreStorage::handle_put_msg(comm_message* msg) {
	// TODO: better equity
	if (addr.sin_port != group.primary.addr.sin_port) { // only the primary broadcasts
		return 0;
	}


	// std::cout << "STORAGE[" << addr.sin_port << "]: broadcasting key to group\n";
	char buffer[BUFFER_SZE];
	for (int i=0; i < group.num_neighbors; ++i) {
		auto node = group.neighbors[i];

		if (node.alive == false) { 
			// std::cout << "STORAGE[" << addr.sin_port << "]: found dead node, skipping\n";
			continue;
		}
		struct sockaddr_in addr = node.addr;
		/*
		addr.sin_family = AF_INET;
		addr.sin_port = htons(node.addr.sin_port);

		// TODO: set IP correctly
		if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
			printf("Invalid address/ Address not supported \n");
			return -1;
		}
		*/
		int fd;
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("STORAGE: neighbor socket");
			return -1;
		}

		if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			if (errno == ETIMEDOUT) {
				std::cerr << "STORAGE[" << addr.sin_port << "]: neighbor died" << std::endl;

				if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
					perror("STORAGE: put finalize ack connection");
					return false;
				}

				node_failure_message response_message;
				response_message.type = NODE_FAILURE;
				response_message.node = node;
				if (send(this->connect_fd, &response_message, sizeof(response_message), 0) < 0) {
					perror("STORAGE: put send failure message failed");
					return false;
				} 

				if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
					perror("SERVER: put read storage failed");
					return false;
				}

				if (((generic_message *) buffer)->type != ACK) {
					std::cerr << "STORAGE[" << addr.sin_port << "]: manager didnt respond w ack after death notification\n";
				}
			}

			perror("STORAGE: put storage connect failed");
			continue;
		}

		// if connection succeeds, send put(key, value) to storage node
		if (send(fd, (void*)msg, sizeof(comm_message), 0) < 0) { // doesn't like me using &msg instead of buffer
			perror("STORAGE: put send storage failed");
			return false;
		} 

		if (read(fd, &buffer, sizeof(buffer)) < 0) {
			perror("STORAGE: put read storage failed");
			return false;
		}
		if (((generic_message *) buffer)->type != ACKPUT) {
			std::cerr << "STORAGE[" << addr.sin_port << "]: neighbor didnt respond w ack after death notification\n";
		}
		close(fd);
	}

	return 0;
}
/*
* Sets up listen and connection sockets. Hard binds manager socket to 8080 so it can be discovered by client and node. Does not begin any socket operations
*/
int GTStoreStorage::socket_init() {

	int opt = 1;

	if ((this->listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("STORAGE: listen socket");
		return -1;
	} 

	if ((this->connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("STORAGE: connect socket");
		return -1;
	}
	
	if ((this->neighborhood_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("STORAGE: neighbor socket");
		return -1;
	}

	if (setsockopt(this->listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("STORAGE: Option Selection Failed");
        return -1;
    }
//#struct timeval time_val_struct = { 0 };
//#	time_val_struct.tv_sec = 5;
//#	time_val_struct.tv_usec = 0;
//#	if (setsockopt(this->neighborhood_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
//#		perror("STORAGE: restart timeout opt failed");
//#		return -1;
//#	}
//#
	if (setsockopt(this->neighborhood_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("STORAGE: Option Selection on neighborhood Failed");
        return -1;
    }
    //int addrlen = sizeof(mang_connect_addr);
	mang_connect_addr.sin_family = AF_INET;
    mang_connect_addr.sin_port = htons(PORT);
	if (inet_pton(AF_INET, "127.0.0.1", &mang_connect_addr.sin_addr) // Assumes manager is on a local system. This can obviously be changed to take in a cmdline
																	 // arg to specify destination IP
        <= 0) {
        std::cout << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(0);  // Bind to a random available port

	if (bind(this->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	/*cant connect because we don't know where, but we can start to listen*/
	if (listen(this->listen_fd, 32) < 0) {
		perror("STORAGE: listen failed");
		return -1;
	}

	struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    if (getsockname(this->listen_fd, (struct sockaddr *)&sin, &sinlen) == -1) {
        perror("getsockname");
		return -1; 
	}

	this->addr = sin;

	return 0;
}



int GTStoreStorage::put(std::string key, val_t value) {
	int retVal = 0; // Return 1 if overwriting, 0 if initializing
	if (store.count(key) != 0) retVal = 1;
	store[key] = value;
	load++;

	// std::cout << "STORAGE[" << addr.sin_port << "] store= {";
	// for (auto& kv : store) {
	// 	std::cout << kv.first << " : " << kv.second << ",";
	// }
	// std::cout << "}\n";

	std::cout << "STORAGE[" << addr.sin_port << "] elements= " << store.size() << std::endl;
	return retVal;
}

// This will return a default constructed string of key is not in store
val_t GTStoreStorage::get(std::string key) {
	return store[key];
}

int GTStoreStorage::get_load() {
	return load;
}



int main(int argc, char **argv) {
	GTStoreStorage storage;
	storage.init();

	storage.listen_for_msgs();
}




