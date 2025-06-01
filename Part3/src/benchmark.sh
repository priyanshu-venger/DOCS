#!/bin/bash

# Base directory for storing benchmark results
BASE_DIR="./benchmark"

# Function to run the benchmark and save results
run_benchmark() {
  local request_count=$1
  local parallel_connections=$2
  local result_file=$3

  # Run the redis-benchmark command
  redis-benchmark -h 127.0.0.1 -t 4 -p 6379 -n "$request_count" -c "$parallel_connections" -r 100000000 -t set,get > "$result_file"
  
  # Print the result for verification
  echo "Results saved to $result_file"
}

# Create necessary directories
create_directories() {
  mkdir -p "$BASE_DIR/10000-requests/10"
  mkdir -p "$BASE_DIR/100000-requests/100"
  mkdir -p "$BASE_DIR/1000000-requests/1000"
}

# Run benchmarks for different combinations of requests and connections
run_all_benchmarks() {
  # 10,000 requests
  #run_benchmark 10000 10 "$BASE_DIR/10000-requests/10/results.txt"
  
  # 100,000 requests
  #run_benchmark 100000 100 "$BASE_DIR/100000-requests/100/results.txt"
  
  # 1,000,000 requests
  run_benchmark 1000000 1000 "$BASE_DIR/1000000-requests/1000/results.txt"
}

# Main execution
create_directories
run_all_benchmarks

echo "All benchmarks completed."
