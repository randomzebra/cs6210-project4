#include "gtstore.hpp"
#include <map>



void GTStoreStorage::init() {
	cout << "Inside GTStoreStorage::init()\n";
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
	std::cout << "node init" << std::endl;
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
	std::cout << "STORAGE: Node init ack received " << buffer << std::endl;
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


int GTStoreStorage::listen_comms() {
	//Listen for assignment message:
	char buffer[BUFFER_SZE];
	struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    if (getsockname(this->listen_fd, (struct sockaddr *)&sin, &sinlen) == -1) {
        perror("getsockname");
		return -1; 
	}

	while(true) {
		if (accept(this->listen_fd, (struct sockaddr *)&sin, &sinlen) < 0) { //This happens either at the beginning when init groups are assigned or when primary dies
			perror("STORAGE: Discovery accept failed");
		}

		//TODO: PUT, GET, and DISC (assign to primary and give replicas)
	}
	
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
}




