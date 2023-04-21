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
		public:
				void init();
};

class GTStoreStorage {
		private:
				std::map<std::string, val_t> store; //Storage map
				//Nodes should be identified by PID
				int load; //Measure of load on current node
				
		public:
				GTStoreStorage();
				void init();
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
};

#endif
