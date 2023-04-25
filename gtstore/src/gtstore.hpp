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
#include <memory>


#define MAX_KEY_BYTE_PER_REQUEST 20
#define MAX_VALUE_BYTE_PER_REQUEST 1000
#define REQUEST_TYPE 32
#define PORT 8080 //This is the manager's port
#define BUFFER_SZE (REQUEST_TYPE + MAX_KEY_BYTE_PER_REQUEST + MAX_VALUE_BYTE_PER_REQUEST + 100) //+ 100 for delimiters

using namespace std;

typedef string val_t; //Per Piazza @327, this is ok, and 

struct node_t {
	struct sockaddr_in addr;
	bool alive;

	bool operator==(const node_t &n) const {
		return n.addr.sin_port == addr.sin_port && n.addr.sin_addr.s_addr == addr.sin_addr.s_addr;
    }
	bool operator<(const node_t &n) const {
		return n.addr.sin_port < addr.sin_port && n.addr.sin_addr.s_addr < addr.sin_addr.s_addr;
    }
};

typedef struct { 
	node_t primary;
	uint32_t num_neighbors;
	node_t neighbors[25]; //Assume a hardcap of 25 replicas
} store_grp_t;

enum MSG_TYPE {
	DISC, //Storage discovery
	PUT, // PUT a key
	GET, // GET a key
	NODE_FAILURE, // A node has failed
	HEARTBEAT,
	ACKPUT,
	ACKGET,
	ACK,
	FAIL,
	S_INIT	// Storage init (who is primary, who are neighbors) // TODO: rename STORAGE_GRP
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


static void print_node(node_t n) {
	std::cout << "(" << n.addr.sin_addr.s_addr << ":" << n.addr.sin_port << ", alive=" << (n.alive ? "1": "0") << ")";
}
static void print_group(store_grp_t group) {
	std::cout << "primary=(";
	print_node(group.primary);
	std::cout << ") num_neighbors=" << group.num_neighbors << " neighbors=(";
	for (uint32_t i=0; i < group.num_neighbors; ++i) {
		print_node(group.neighbors[i]);
		std::cout << ",";
	}
	std::cout << "\n";
}

struct discovery_message {
	uint8_t type;
	node_t node;
};

struct assignment_message {
	uint8_t type;
	store_grp_t group;

	assignment_message(): type{}, group{} {}
	assignment_message(store_grp_t group): type{S_INIT}, group{group} {}
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

struct node_failure_message {
	uint8_t type;
	char key[MAX_KEY_BYTE_PER_REQUEST]; // TODO: could be relevant
	node_t node;
};

class GTStoreClient {
		private:
				int client_id;
				val_t value;
				int listen_fd, connect_fd;
				int mngr_socket_fd;
				struct sockaddr_in mang_connect_addr;
		public:
				void init(int id);
				int socket_init();
				int restart_connection(int mode);
				void finalize();
				vector<string> get(string key);
				bool put(string key, vector<string> value);
				node_t last_p_node; // the last primary node connected
};

class GTStoreManager {
		private:
				vector<node_t> uninitialized; //Ports's of uninitialized nodes
				queue<std::shared_ptr<store_grp_t>> rr; //Round robin for load balancing. Acts as lookup for dead nodes.
				std::vector<std::shared_ptr<store_grp_t>> groups;
				std::map<std::string, std::shared_ptr<store_grp_t>> key_group_map;
				std::map<node_t, std::vector<std::string>> node_keys_map;
				int total_nodes;
				int k;
				bool live;
				int listen_fd, connect_fd;
				struct sockaddr_in listen_addr; //Server listening socket
				std::mutex rr_mutex;
				void push_group_assignments();
				int commit_push(string key, store_grp_t * strgrp);
				int restart_connection(int mode);
		public:
				void init(int nodes, int k);
				std::shared_ptr<store_grp_t> put(std::string key, val_t val); //Should implement uninitialized/RR logic
				int put_network(std::string key, val_t val); //Should implement uninitialized/RR logic
				void comDemux(char* buffer, sockaddr_in* sin, int client_fd);
				int handle_node_failure(node_t node);
				int socket_init();
				int node_init();
				int listen_for_msgs();
				int handle_put(comm_message* msg);
};

class GTStoreStorage {
		private:
				std::map<std::string, val_t> store; //Storage map
				//Nodes should be identified by PID
				int load; //Measure of load on current node
				int listen_fd, connect_fd, neighborhood_fd;
				struct sockaddr_in addr;
				struct sockaddr_in mang_connect_addr;
				store_grp_t group; //All replicas, excluding itself.
				int handle_put_msg(comm_message* msg);
				int com_demux(char* buffer, int client_fd);
		public:
				void init();

				int socket_init();
				int node_init();
				int put(std::string key, val_t value);
				val_t get(std::string key);
				bool leader; //Is leader or replica
				int get_load();
				enum node_state {
					UNINITIALIZED,
					LEADER,
					REPLICA
				};
				node_state state;
				int listen_for_msgs();
};

#endif
