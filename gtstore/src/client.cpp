#include "gtstore.hpp"

void GTStoreClient::init(int id) {
		client_id = id;
		socket_init();
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
	
	/*
	if ((this->mngr_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("STORAGE: connect manager socket");
		return -1;
	}
	*/

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

		//cout << "Inside GTStoreClient::get() for client: " << client_id << " key: " << key << "\n";
		vector<string> value;
		// Get the value!
		if (connect(this->connect_fd, (struct sockaddr*) &this->mang_connect_addr, sizeof(this->mang_connect_addr)) < 0) {
			perror("CLIENT: get connect failed");
			return {};
		}

		comm_message msg;
		msg.type = GET;
		strcpy(msg.key, key.c_str());

		//Send message to manager
		if (send(this->connect_fd, &msg, sizeof(msg), 0) < 0) {
			perror("CLIENT: get send mngr failed");
			return {};
		} 

		char buffer[BUFFER_SZE];
		if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
			perror("CLIENT: get read mngr failed");
			return {};
		}

		if (((generic_message *) buffer)->type == S_INIT) {
			auto res_msg = (assignment_message *) buffer;
			/*
			std::cout << "[CLIENT] recieved response from GET, primary port=";
			print_node(res_msg->group.primary);
			std::cout << "\n";
			*/

			if(restart_connection(1) < 0) { //Restart w/ timeout
				std::cerr << "Failed to restart connection after get mangr ACK" << std::endl;
				return {};
			}

			sockaddr_in in_addr = res_msg->group.primary.addr;
			
			if (connect(this->connect_fd, (struct sockaddr*) &in_addr, sizeof(in_addr)) < 0) {
				if (errno == ETIMEDOUT) { //Primary timed out, dead
					std::cerr << "Primary dead" << std::endl;
					// connect back to manager
					restart_connection(0);
					if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
						perror("CLIENT: GET finalize ack connection");
						return {};
					}

					node_failure_message response_message;
					response_message.type = NODE_FAILURE;
					response_message.node = res_msg->group.primary;
					if (send(this->connect_fd, &response_message, sizeof(response_message), 0) < 0) {
						perror("CLIENT: get send failure message failed");
						return {};
					} 

					if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
						perror("CLIENT: put read storage failed");
						return {};
					}

					// TODO: maybe not... retry the same process again?
					// the manager responds with an ACK after we tell it a node has died
					if (((generic_message *) buffer)->type == ACKGET) {//Manager has purged the failing port from records
						restart_connection(0);
						return {};
					}
				}

				perror("CLIENT: get storage connect failed");
				// TODO: recall put (key, value)
				return {};
		}

		// if connection succeeds, send put(key, value) to storage node
		if (send(this->connect_fd, (void*)&msg, sizeof(msg), 0) < 0) { // doesn't like me using &msg instead of buffer
			perror("CLIENT: get send storage failed");
			return {};
		} 

		if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
			perror("CLIENT: get read storage failed");
			return {};
		}

		// ACKPUT means the primary storage node has recieved our key value
		if (((generic_message *) buffer)->type == ACKGET) { // Put succeeded on primary, Send ACKPUT to mngr to update Map.
			// doing this to close the storage node file descriptor because we want to connect to the manager 
			if(restart_connection(0) < 0) {
				std::cerr << "Failed to restart connection after put storage ack" << std::endl;
				return {};
			}

			auto values = ((comm_message*)buffer)->value;
			std::cerr << "CLIENT: values: " << values << "\n";

			// TODO: deliminate
		} else {
			std::cerr << "CLIENT GET: empty" << std::endl;
		}
		
	} else {
		std::cerr << "CLIENT: recieved unknown packet, expecting S_INIT\n";
	}

	return {};

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
		close(this->connect_fd);
		return false;
	} 

	char buffer[BUFFER_SZE];
	if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
		perror("CLIENT: put read mngr failed");
		close(this->connect_fd);
		return false;
	}

	if (((generic_message *) buffer)->type == S_INIT) {
		auto res_msg = (assignment_message *) buffer;
		//std::cout << "[CLIENT] recieved response from PUT, primary port=" << res_msg->group.primary << "\n";

		if(restart_connection(1) < 0) { //Restart w/ timeout
			std::cerr << "Failed to restart connection after put mangr ACK" << std::endl;
			return false;
		}

		/*
		std::cout << "CLIENT: connecting to ";
		print_node(res_msg->group.primary);
		std::cout << "\n";
		*/

		struct sockaddr_in in_addr = res_msg->group.primary.addr;

		if (connect(this->connect_fd, (struct sockaddr*) &in_addr, sizeof(in_addr)) < 0) {
			if (errno == ETIMEDOUT || errno == ECONNREFUSED) { // fail no matter what!
				std::cerr << "Primary dead" << std::endl;
				// connect back to manager
				restart_connection(0);
				if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
					perror("CLIENT: put finalize ack connection");
					return false;
				}

				node_failure_message response_message;
				response_message.type = NODE_FAILURE;
				response_message.node = res_msg->group.primary;
				if (send(this->connect_fd, &response_message, sizeof(response_message), 0) < 0) {
					perror("CLIENT: put send failure message failed");
					close(this->connect_fd);
					return false;
				} 

				if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
					perror("CLIENT: put read storage failed");
					close(this->connect_fd);
					return false;
				}

				// TODO: maybe not... retry the same process again?
				// the manager responds with an ACK after we tell it a node has died
				//if (((generic_message *) buffer)->type == ACKPUT) {//Manager has purged the failing port from records
					//restart_connection(0);
					//return false;
				//}
			}

			perror("CLIENT: put storage connect failed");
			close(this->connect_fd);
			// TODO: recall put (key, value)
			return false;
		}

		// if connection succeeds, send put(key, value) to storage node
		if (send(this->connect_fd, (void*)&msg, sizeof(msg), 0) < 0) { // doesn't like me using &msg instead of buffer
			perror("CLIENT: put send storage failed");
			if (errno == ETIMEDOUT || errno == ECONNREFUSED) { // fail no matter what!
				restart_connection(0);
				if (connect(this->connect_fd, (struct sockaddr*) &mang_connect_addr, sizeof(mang_connect_addr)) < 0) {
					perror("CLIENT: put finalize ack connection");
					return false;
				}

				node_failure_message response_message;
				response_message.type = NODE_FAILURE;
				response_message.node = res_msg->group.primary;
				if (send(this->connect_fd, &response_message, sizeof(response_message), 0) < 0) {
					perror("CLIENT: put send failure message failed");
					return false;
				} 

				if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
					perror("SERVER: put read storage failed");
					restart_connection(0);
					return false;
				}

				if (((generic_message *) buffer)->type != ACK) {
					std::cerr << "[CLIENT]: manager didnt respond w ack after death notification\n";
				}
				restart_connection(0);

				return false;
			} else {
				std::cerr << "[CLIENT]: unknown error when connecting to neigbor" << std::endl;
				restart_connection(0);
				return false;
			}
		}

		if (read(this->connect_fd, &buffer, sizeof(buffer)) < 0) {
			perror("CLIENT: put read storage failed");
			restart_connection(0);
			return false;
		}

		// ACKPUT means the primary storage node has recieved our key value
		if (((generic_message *) buffer)->type == ACKPUT) { // Put succeeded on primary, Send ACKPUT to mngr to update Map.
			// doing this to close the storage node file descriptor because we want to connect to the manager 
			if(restart_connection(0) < 0) {
				std::cerr << "Failed to restart connection after put storage ack" << std::endl;
				return false;
			}

			// TODO: make new message: type port message
			//
			// manager needs to add key for that storage group
			/*
			msg->type = ACKPUT;
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
			*/
		}
		
	} else {
		std::cerr << "CLIENT: recieved unknown packet, expecting S_INIT\n";
	}

	return true;
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
	close(this->listen_fd);
	close(this->connect_fd);
	cout << "Inside GTStoreClient::finalize() for client " << client_id << "\n";
}
