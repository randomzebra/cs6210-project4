### Manager
Manager is responsible for maintaining mapping of keys to storage PIDS, and making sure they are alive. It should be able to/contain

* List of uninitialized node PIDS (for the initial puts). It should be N length, and every time a put request comes in, it should take k + 1 nodes out (1 primary, k replicas) and initialize those nodes as replicas or primaries respectively. It should do this until empty

* a new data structure that is a vector of PIDs. This represents a primary-replica chain. We can use this in both the key ->storage group mapping and the round robin for load balancing

* Round robin of storage groups. When a put comes in with a key with no existing mapping, it should pull a storage group from the RR and use that. The replication process should be handled by the primary. 

* key ->storage group mapping for put/get requests that have been done before. This is how we handle gets, and how we handle overrides.

* heartbeat ping to determine if nodes have died. This is 1 of 3 methods we need to handle node death. The full list of three are:
    1. Manager heartbeat ping is missed some number of times
    2. Client times out a request
    3. Primary storage node times out a request (during put propagation)

* logic for replica recovery. Manager receiver a signal that a node has died from 1 of 3 ways. if node is a primary, it should set the next replica in line in the chain to be the new primary. If it is a replica, boot it from the group. If the group is now empty, fail out: a fatal crash has occurred.

### Storage Node

Storage nodes contain the actual key value maps. They should contain:

* The map itself
* if it is a primary:
    * some way of getting its replica list from the manager, or keep a list of replicas itself.
    * A put function that propagates the put to its replicas
    * A way of notifying the manager if one of its replicas has died

Client should contact the storage node directly for gets and sets. The same signaling method that we use to signal the primary to put can be used to signal the replicas, we just don't need to propagate gets. 

### Client

Client acts as the intermediary between the internals of GTStore. It should: 

* handle put and get ops for a actual user program
    * This is the code that should actually send messages to the manager and storage nodes, and pass that info back to the user program
* We only ever need to handle one client operation.
* Does a manager need to lock a storage group when a request is occurring?


### Communication

At first, was thinking about using PIDs to identify processes, but our communication must be network. We can use any RCP framework, (gRCP is the recommended), or communicate through sockets. Need input on which to use.