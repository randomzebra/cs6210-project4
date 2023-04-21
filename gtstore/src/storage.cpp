#include "gtstore.hpp"
#include <map>



void GTStoreStorage::init() {
	cout << "Inside GTStoreStorage::init()\n";
	load = 0;
	state = UNINITIALIZED; //Leader is set upon first put to a node. Replica set by manager when another node is set as leader
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


int main(int argc, char **argv) {
	GTStoreStorage storage;
	storage.init();
}




