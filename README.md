Objective
This project aims to explore file handling, multithreaded programming, and performance optimization by implementing a multithreaded web crawler. The program generates a histogram of characters present in text files fetched from URLs.

Key Goals:
Efficiently process large datasets.
Use multithreading to improve performance.
Optimize code for maximum efficiency.
Features
Multithreaded Web Crawling: Fetch and process text files from URLs using libcurl.
Histogram Generation: Count words starting with each letter of the English alphabet.
Thread-Safe Updates: Use pthread_mutex to ensure thread-safe access to shared data.
Error Handling: Gracefully handle invalid URLs, timeouts, and failed requests.
Usage
Run the program with the following command-line arguments:

Path to text file: File containing URLs (one per line).
Number of threads: Total threads for processing.
Batch size (X): Number of words processed per batch.
Example:
./web_crawler urls.txt 4 1024
Execution Configurations
Tested Configurations:
Thread Counts: 1, 2, 4, 8
Batch Sizes: 1, 32, 256, 1024, 4096

Observations:
Best performance achieved with 4 threads and a batch size of 1024.
Performance degraded with 8 threads due to CPU oversubscription.
Optimization Steps
Thread Pooling: Balanced workload across threads.
Batch Processing: Reduced frequent locking by processing words in batches.
Thread Safety: Used pthread_mutex for synchronized updates.
Retry Mechanism: Implemented for failed HTTP requests.
Efficient Memory Management: Dynamic allocation and deallocation of memory.
Results
A timing table and graph were generated to analyze performance across configurations. Key insights:

Smaller batch sizes cause higher overhead.
Larger batch sizes increase contention for shared resources.
Optimal threading occurs at 4 threads with balanced workload distribution.
Conclusion
This project successfully demonstrates the power of multithreading and synchronization in handling large datasets. By balancing parallelism, workload distribution, and resource contention, the program achieves efficient performance for character histogram generation.

