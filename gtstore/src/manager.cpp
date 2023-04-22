#include "gtstore.hpp"

void GTStoreManager::init(int nodes, int k) {
	
	cout << "Inside GTStoreManager::init()\n";
	this->total_nodes = nodes;
	this->k = k;

	//TODO: socket code here
}


store_grp_t GTStoreManager::put(std::string key, val_t val) {
	if (uninitialized.size() > 0) {
		store_grp_t new_grp;
		for (int i = 0; i < uninitialized.size() && i < k; i++) {
			new_grp.push_back(uninitialized.back());
			uninitialized.pop_back();
		}
		rr.push(&new_grp);
		return new_grp;
	} 

	store_grp_t *existing_grp;
	auto record = existing_puts.find(key);
	
	if (record == existing_puts.end()) { //No existing put found, allocate existing grp w/ RR
		if (rr.size() == 0) {
			return {}; //Failure, if there are no allocated nodes and no rr groups, we have zero nodes
		}

		existing_grp = rr.front(); //RR
		rr.pop();
		rr.push(existing_grp);

		existing_puts.insert(std::pair<std::string, store_grp_t *>(key, existing_grp)); //Add a new put entry
		return *existing_grp;
	} else {
		return *record->second; //else return the found grp.
	}
}




int main(int argc, char **argv) {

	GTStoreManager manager;
	manager.init(atoi(argv[1]), atoi(argv[2]));
	
}
