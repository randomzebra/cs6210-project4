#include "gtstore.hpp"

void GTStoreClient::init(int id) {

		cout << "Inside GTStoreClient::init() for client " << id << "\n";
		client_id = id;
}


/*
* Sets up listen and connection sockets. Hard binds manager socket to 8080 so it can be discovered by client and node. Does not begin any socket operations
*/
int GTStoreClient::socket_init() {

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

	return 0;
}

vector<string> GTStoreClient::get(string key) {

		cout << "Inside GTStoreClient::get() for client: " << client_id << " key: " << key << "\n";
		vector<string> value;
		// Get the value!
		return value;
}

bool GTStoreClient::put(string key, vector<string> value) {

	string print_value = "";
	string serialized_value = "";
	if (value.size() > 100) {
		std::cerr << "Max value size is 100, we don't have enough space for more than 100 delimeters" << std::endl;
		return false;
	}

	for (uint i = 0; i < value.size(); i++) {
			print_value += value[i] + " ";
			serialized_value += value[i] + "|"; //Pipes are less common, serialize this way
	}
	cout << "Inside GTStoreClient::put() for client: " << client_id << " key: " << key << " value: " << print_value << "\n";
	// Put the value!

	if (connect(this->connect_fd, (struct sockaddr*) &this->mang_connect_addr, sizeof(this->mang_connect_addr)) < 0) {
		perror("CLIENT: put connect failed");
		return false;
	}

	comm_message msg;
	msg.type = PUT;
	strcpy(msg.key, key.c_str());
	strcpy(msg.value, serialized_value.c_str());

	//Send message to manager
	if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
		perror("CLIENT: put send mngr failed");
		return false;
	} 

	char buffer[BUFFER_SZE];
	if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
		perror("CLIENT: put read mngr failed");
		return false;
	}
	
	if (((generic_message *) buffer)->type == ACKPUT) { //Received an ACK w/ a storage port from mngr
		port_message * mng_resp = (port_message *) buffer;
		if(restart_connection(1) < 0) { //Restart w/ timeout
			std::cerr << "Failed to restart connection after put mangr ACK" << std::endl;
			return false;
		}

		//Construct storage address w/ port
		struct sockaddr_in in_addr;
		in_addr.sin_family = AF_INET;
		in_addr.sin_port = htons(mng_resp->port);

		if (inet_pton(AF_INET, "127.0.0.1", &in_addr.sin_addr)
			<= 0) {
			printf(
				"\nInvalid address/ Address not supported \n");
			return -1;
		}

		
		if (connect(this->connect_fd, (struct sockaddr*) &in_addr, sizeof(in_addr)) < 0) {
			if (errno == ETIMEDOUT) { //Primary timed out, dead
				std::cerr << "Primary dead" << std::endl;
				restart_connection(0);
				if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
					perror("CLIENT: put finalize ack connection");
					return false;
				}

				mng_resp->type = FAIL;
				if (send(this->connect_fd, mng_resp, sizeof(&mng_resp), 0) < 0) {
					perror("CLIENT: put send mngr failed");
					return false;
				} 

				if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
					perror("CLIENT: put read storage failed");
					return false;
				}

				if (((generic_message *) buffer)->type == ACKPUT) {//Manager has purged the failing port from records
					restart_connection(0);
					return false;
				}
			}

			perror("CLIENT: put storage connect failed");
			return false;
		}

		if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
			perror("CLIENT: put send storage failed");
			return false;
		} 

		if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
			perror("CLIENT: put read storage failed");
			return false;
		}

		if (((generic_message *) buffer)->type == ACKPUT) { // Put succeeded on primary, Send ACKPUT to mngr to update Map.
			if(restart_connection(0) < 0) {
				std::cerr << "Failed to restart connection after put storage ack" << std::endl;
				return false;
			}

			msg.type = ACKPUT;
			if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
				perror("CLIENT: put finalize ack connection");
				return false;
			}

			if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
				perror("CLIENT: put finalize ack send");
				return false;
			} 

			if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
				perror("CLIENT: put finalize ack read");
				return false;
			}

			if (((generic_message *) buffer)->type == ACKPUT) { // We're done
				restart_connection(0);
				return true;
			}
		}
		
	}

	return false;

}

int GTStoreClient::restart_connection(int mode) { //0 for no timeout, w/ 5 second timeout
	close(this->connect_fd);
	if ((this->connect_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("ClIENT: restart connection failed");
		return -1;
	}
	if (mode == 1) {
		struct timeval time_val_struct = { 0 };
		time_val_struct.tv_sec = 5;
		time_val_struct.tv_usec = 0;
		std::cout << "node init" << std::endl;
		if (setsockopt(this->connect_fd, SOL_SOCKET, SO_RCVTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
			perror("CLIENT: restart timeout opt failed");
			return -1;
		}

		if (setsockopt(this->connect_fd, SOL_SOCKET, SO_SNDTIMEO, &time_val_struct, sizeof(time_val_struct)) < 0) {
			perror("CLIENT: restart timeout opt failed");
			return -1;
		}	
		}
	
	return 0;
}





void GTStoreClient::finalize() {

		cout << "Inside GTStoreClient::finalize() for client " << client_id << "\n";
}
