#include "gtstore.hpp"
#include <chrono>

using namespace std::chrono;



int main() {
    string message = "Testing 100k put ops for load balance.";
    std::cout <<  message << std::endl;
    int i;
    int num_ops = 100000;

    GTStoreClient client;
    client.init(0);

    string key = "key";
    auto start = high_resolution_clock::now();

    for (i = 0; i < num_ops; ++i) {
        vector<string> values;
        values.push_back(std::string{"val" + std::to_string(i)});
        values.push_back(std::string{"val" + std::to_string(i + 1)});
        std::string newKey{key + std::to_string(i)};
        if (!client.put(newKey, values)) {
            std::cerr << "Error in put operation: " << std::to_string(i) << std::endl;
            return -1;
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    std::cout << "Load Balance Test: " << (num_ops * 2) << " operations completed in " << (duration.count()/1e6) << " seconds using std::chrono" << std::endl;

    return 0;
}