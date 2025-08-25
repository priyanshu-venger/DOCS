This repository contains the source code and documentation for DOCS DB, a high-performance, standalone key-value database developed as part of the Design Optimization of Computing Systems (DOCS) course assignment.

Project Overview
The project is implemented in three distinct parts:

    A standalone, multi-threaded storage engine with a standard single threaded client.

    An exploration of network stacks.

    Integration of the storage engine and network layer into a complete database server that communicates using the Redis (RESP2) protocol.

Key Features

    The database leverages LSM Tree and is optimized for read-heavy.
    Designed to leverage multiple CPU cores for concurrent request processing.
    Implements the RESP2 wire protocol, allowing it to be benchmarked with the standard redis-benchmark utility.
    Includes Doxygen-generated documentation.

Setup and Execution Instructions

Part A: Key-Value Storage Engines

This part implements the database as a static library.

Run the following commands to test the implementation:
	Navigate to the directory:
	cd Part1
	Build the library:
	make
	Run:
	g++ -o main.cpp -L. -ldatabase
	./main
You'll get an interactive interface for testing.

Part B: Network Exploration

This part explores and benchmarks TCP networking.
Follow the following steps to test the implementation:
	Navigate to the directory:
	cd Part2
	Create and configure network namespaces:
	(Note: This requires sudo privileges.)
	sudo ./script.sh

	Run server and client:
	g++ -o server server.cpp
	./server
	g++ -o client.cpp
	./client
	
Part 3: DOCS DB - The Complete Database

This part integrates the storage engine (Part A) with the chosen network stack (Part B).

	Navigate to the directory:
	cd Part3

	Build the project:
	make

	Run the DOCS DB Server:
	g++ -o server server.cpp -L. -ldatabase
	./server

	Run the client:
	g++ client.cpp
	./a.out

	Otherwise if you want to test with redis benchmark, run:
	./benchmark.sh		#Uncomment what concurrency you want to run it for
	Results will get stored in the benchmark folder

Design Documents
Detailed design documents for Part A and Part C can be found in the docs/ subdirectory of each respective part. These documents outline key engineering decisions, data structures, concurrency models, and optimizations implemented.

Documentation
    Documentation: Doxygen-generated documentation (in both PDF and HTML format) is available in the docs/ subdirectory for Part A and Part C.
