#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <map>
#include <queue>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <sys/socket.h>


#define MAX_KEY_BYTE_PER_REQUEST 20
#define MAX_VALUE_BYTE_PER_REQUEST 1000
#define REQUEST_TYPE 32
#define PORT 8080 //This is the manager's port
#define BUFFER_SZE (REQUEST_TYPE + MAX_KEY_BYTE_PER_REQUEST + MAX_VALUE_BYTE_PER_REQUEST + 100) //+ 100 for delimiters

using namespace std;

typedef string val_t; //Per Piazza @327, this is ok, and 
typedef vector<uint32_t> store_grp_t;

enum MSG_TYPE {
	DISC, //Storage discovery
	PUT,
	GET,
	HEARTBEAT,
	ACKPUT,
	ACKGET,
	FAIL
};

/*
* ACK type: 
* For Replica to primary:
* 	ACK means a put has succeeded for key. Failure is a timeout
* For primary to client:
*   ACK means a put has succeeded for key, only primary. Client does not care about replica failures. Failure is a timeout
* For primary to manager:
*   No acks or fails sent. Send fail_message instead to manager w/ failing replica(s)
* For client to manager:
*   ACK means a put has acquired ack from storage, which means that put has succeeded. Manager should map key to group. Timeout 
*   should send a fail_message to manager w/ failing primary
* 
*/

struct discovery_message{
	uint8_t type;
	uint32_t discovery_port;
};

struct assignment_message {
	uint8_t type;
	uint32_t grp_assigment[25]; //Assume a hardcap of 25 replicas
};

//Used to demux message type
struct generic_message {
	uint8_t type;
};
struct port_message {
	uint8_t type;
	uint32_t port;
	char key[MAX_KEY_BYTE_PER_REQUEST];
};
struct comm_message {
	uint8_t type;
	char key[MAX_KEY_BYTE_PER_REQUEST];
	char value[MAX_KEY_BYTE_PER_REQUEST];
};

class GTStoreClient {
		private:
				int client_id;
				val_t value;
				int listen_fd, connect_fd;
				struct sockaddr_in mang_connect_addr;
	

		public:
				void init(int id);
				int socket_init();
				int restart_connection(int mode);
				void finalize();
				vector<string> get(string key);
				bool put(string key, vector<string> value);
};

class GTStoreManager {
		private:
				vector<uint32_t> uninitialized; //Ports's of uninitialized nodes
				queue<store_grp_t *> rr; //Round robin for load balancing. Acts as lookup for dead nodes.
				std::map<std::string, store_grp_t *> existing_puts;
				int total_nodes;
				int k;
				bool live;
				int listen_fd, connect_fd;
				struct sockaddr_in listen_addr; //Server listening socket
				std::mutex rr_mutex;
				void push_group_assignments(vector<store_grp_t> grp_assignments);
				int commit_push(string key, store_grp_t * strgrp);
				int restart_connection(int mode);
		public:
				void init(int nodes, int k);
				store_grp_t put(std::string key, val_t val); //Should implement uninitialized/RR logic
				int put_network(std::string key, val_t val); //Should implement uninitialized/RR logic
				void comDemux();
				int socket_init();
				int node_init();
				int listen_for_coms();
				


				//gets are not handled here.
				//int hearbeat() // should return a dead pid. Should run in a seperate thread? 
};

class GTStoreStorage {
		private:
				std::map<std::string, val_t> store; //Storage map
				//Nodes should be identified by PID
				int load; //Measure of load on current node
				int listen_fd, connect_fd;
				uint32_t listen_port;
				struct sockaddr_in mang_connect_addr;
				store_grp_t replicas; //All replicas, excluding itself.
				int listen_comms();
		public:
				void init();

				int socket_init();
				int node_init();
				int put(std::string key, val_t value); //TODO: implement replica propagation
				val_t get(std::string key);
				bool leader; //Is leader or replica
				int get_load();
				enum node_state {
					UNINITIALIZED,
					LEADER,
					REPLICA
				};
				node_state state;
};

#endif
