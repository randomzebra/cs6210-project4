#include <map>
#include <queue>
#include <vector>
#include <string>
#include <iostream>

typedef std::vector<std::string> value_t;

int main(int argc, char **argv) {
    std::map<std::string, value_t *> tMap;
    std::queue<value_t *> tQueue;

    value_t value;
    value.push_back("hello");
    value.push_back("world");

    tQueue.push(&value);

    value_t *trans_val = tQueue.front();
    tQueue.pop();
    tQueue.push(trans_val);

    tMap.insert(std::pair<std::string, value_t *>("a", trans_val));
    
    trans_val->push_back("test1");
    trans_val->push_back("test2");

    std::cout << "trans_val ";

    for (auto it =trans_val->begin(); it != trans_val->end(); it++) {
        std::cout << *it << ' ';
    }
    std::cout << std::endl;

    std::cout << "queue_val ";
    for (auto it = tQueue.front()->begin(); it != tQueue.front()->end(); it++) {
        std::cout << *it << ' ';
    }
    std::cout << std::endl;

}