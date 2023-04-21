#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <map>

#define MAX_KEY_BYTE_PER_REQUEST 20
#define MAX_VALUE_BYTE_PER_REQUEST 1000

using namespace std;

typedef vector<string> val_t;
typedef vector<int> store_grp_t;

class GTStoreClient {
		private:
				int client_id;
				val_t value;
		public:
				void init(int id);
				void finalize();
				val_t get(string key);
				bool put(string key, val_t value);
};

class GTStoreManager {
		private:
				vector<int> uninitialized; //PIDS of uninitialized nodes
				vector<store_grp_t> rr; //Round robin for load balancing. Acts as lookup for dead nodes.
				std::map<std::string, store_grp_t> existing_puts;

		public:
				void init();
				//store_grp_t put(std::string key, val_t val) //Should implement uninitialized/RR logic
				//gets are not handled here.
				//int hearbeat() // should return a dead pid. Should run in a seperate thread? 
};

class GTStoreStorage {
		private:
				std::map<std::string, val_t> store; //Storage map
				//Nodes should be identified by PID
				int load; //Measure of load on current node
				
		public:
				GTStoreStorage();
				void init();
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
