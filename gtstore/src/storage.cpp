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
	
	discovery_message msg;
	msg.type = DISC;
	msg.discovery_port = this->listen_port;

	if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
		perror("STORAGE: Node_init send failed");
		return -1;
	}
	char buffer[BUFFER_SZE] = {0};
	read(this->connect_fd, buffer, sizeof(buffer));
	//std::cout << "STORAGE: Node init ack received '" << buffer << "'\n";
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


// handle init messages
int GTStoreStorage::handle_assignment_msg(char* buffer) {
		assignment_message *msg = (assignment_message *) buffer;
		std::cout << "STORAGE: recieved assignment msg ";
		print_group(msg->group);

		return 0;
}
// do something similar in manager
//
// - make new thread with msg demux
// - pick what to do with thajt
int GTStoreStorage::listen_for_msgs() {

	// if i recieve a discovery message, make myself primary
	//
	//
	// 2 types of discover msg:
	// 	1. storage node, saying i'm avaible to the manager
	// 	2. manager sending message to storage node saying you're primary now
	//
	//
	// for type two:
	// 	- cast into dicovery message struct
	// 	- pull info out
	// 	- populate replicas
	// 	- connect to replicas
	//Listen for assignment message:

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
	std::cout << "socket address: " << inet_ntoa(sin.sin_addr) << " port: " << ntohs(sin.sin_port) << "\n";

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
			continue;
		}

		std::cout << "received message: " << buffer << "\n";

		generic_message *msg = (generic_message *) buffer;
		switch(msg->type) {
			case S_INIT:
				std::cout << "STORAGE: init message recieved\n";
				if (handle_assignment_msg(buffer) == -1) {
					std::cout << "STORAGE: handle init msg failed\n";
					return -1;
				}
				continue;
			case PUT:
				std::cout << "STORAGE: put message recieved: TODO (" << buffer << ")\n";
				continue;
			case GET:
				std::cout << "STORAGE: get message recieved: TODO (" << buffer << ")\n";
				continue;
			default:
				std::cout << "STORAGE: unhandled message recieved\n";
				return -1;
		}
		//TODO: PUT, GET, and DISC (assign to primary and give replicas)

		close(client_fd);
	}

	/*
	char* buffer = (char*)malloc(BUFFER_SZE * sizeof(char));
	if (!buffer) {
		perror("listen_for_msgs, buffer malloc");
		return -1;
	}

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = this->listen_port;

    socklen_t sinlen = sizeof(sin);
	int sock_num;
    sock_num = getsockname(this->listen_fd, (struct sockaddr *)&sin, &sinlen);
	if (sock_num == -1) {
        perror("getsockname");
		return -1; 
	}
	std::cout << "sock num: " << sock_num << "\n";

	while(true) {
		if (accept(this->listen_fd, (struct sockaddr *)&sin, &sinlen) < 0) { //This happens either at the beginning when init groups are assigned or when primary dies
			perror("STORAGE: Discovery accept failed");
		}

		if (read(this->listen_fd, buffer, sizeof(buffer)) < 0) {
			perror("STORAGE: Node init Read failed");
			return -1;
		}
		std::cout << "STORAGE: recv message: '" << buffer << "'\n";

		generic_message *msg = (generic_message *) buffer;
		switch(msg->type) {
			case S_INIT:
				std::cout << "STORAGE: init message recieved\n";
				if (handle_assignment_msg(buffer) == -1) {
					std::cout << "STORAGE: handle init msg failed\n";
					return -1;
				}
				continue;
			case PUT:
				std::cout << "STORAGE: put message recieved: TODO (" << buffer << ")\n";
				continue;
			case GET:
				std::cout << "STORAGE: get message recieved: TODO (" << buffer << ")\n";
				continue;
			default:
				std::cout << "STORAGE: unhandled message recieved\n";
				return -1;
		}
		//TODO: PUT, GET, and DISC (assign to primary and give replicas)
	}
	*/
	
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

	if (setsockopt(this->listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("STORAGE: Option Selection Failed");
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
	} else
        this->listen_port = ntohs(sin.sin_port);
	return 0;
}



int GTStoreStorage::put(std::string key, val_t value) {
	int retVal = 0; // Return 1 if overwriting, 0 if initializing
	if (store.count(key) != 0) retVal = 1;
	store[key] = value;
	load++;
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




