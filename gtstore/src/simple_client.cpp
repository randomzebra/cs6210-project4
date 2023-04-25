#include "gtstore.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void put(char* key, char* value) {
	GTStoreClient client;
    client.init(getpid());

    std::vector<std::string> val_vec{value};
	client.put(key, val_vec);

    std::cout << "> Ok, " << client.last_p_node.addr.sin_port << "\n";

	client.finalize();
}

void get(char* key) {
	GTStoreClient client;
    client.init(getpid());

	auto values = client.get(key);

    std::cout << "> " << key << ", ";
    for (const auto& e : values) {
        std::cout << e;
    }
    std::cout << ", " << client.last_p_node.addr.sin_port << "\n";
	client.finalize();
}

int main(int argc, char *argv[]) {
    int c;
    char *key = NULL;
    char *value = NULL;
    int is_put = 0;
    int is_get = 0;

    while (1) {
        static struct option long_options[] = {
            {"put", required_argument, 0, 'p'},
            {"val", required_argument, 0, 'v'},
            {"get", required_argument, 0, 'g'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "p:v:g:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'p':
                is_put = 1;
                key = optarg;
                break;
            case 'v':
                value = optarg;
                break;
            case 'g':
                is_get = 1;
                key = optarg;
                break;
            case '?':
                break;
            default:
                abort();
        }
    }

    if (is_put) {
        if (key == NULL || value == NULL) {
            printf("Error: --put option requires both --key and --val arguments\n");
            exit(EXIT_FAILURE);
        }

        put(key, value);
    } else if (is_get) {
        if (key == NULL) {
            printf("Error: --get option requires a --key argument\n");
            exit(EXIT_FAILURE);
        }

        get(key);
    } else {
        printf("Error: must specify either --put or --get option\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
