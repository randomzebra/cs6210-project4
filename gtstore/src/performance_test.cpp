#include "gtstore.hpp"
#include <chrono>

using namespace std::chrono;



int main() {
    std::cout << "Testing 200k put ops for performance." << std::endl;
    int i;
    int num_ops = 100000;

    GTStoreClient client;
    client.init(0);

    string key = "key";
    auto start = high_resolution_clock::now();

    for (i = 0; i < num_ops; ++i) {
        vector<string> values;
        values.push_back("val" + std::to_string(i));
        values.push_back("val" + std::to_string(i + 1));
        if (!client.put(key + std::to_string(i), values)) {
            std::cerr << "Error in put operation: " << std::to_string(i) << std::endl;
            return -1;
        }
        client.get(key + std::to_string(i));
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    std::cout << duration.count() << std::endl;

    return 0;
}