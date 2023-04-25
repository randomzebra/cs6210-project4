CFLAGS  =
LFLAGS  =
CC      = g++ -std=c++11
RM      = /bin/rm -rf

TESTS = performance_test test_app manager storage
CLIENT_SRC = src/test_app.cpp src/client.cpp
PERFORMANCE_SRC = src/performance_test.cpp src/client.cpp

all: $(TESTS)

manager: src/manager.cpp
	$(CC) -Wall src/manager.cpp -o bin/manager

storage: src/storage.cpp
	$(CC) -Wall src/storage.cpp -o bin/storage

test_app: 
	$(CC) -Wall $(CLIENT_SRC) -o bin/test_app

performance_test:
	$(CC) -Wall $(CLIENT_SRC) -o bin/performance_test
clean:
	$(RM) *.o $(TESTS)