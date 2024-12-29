#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>
#include <time.h>
#include <cairo/cairo.h>


#define ALPHABET_COUNT 26

int word_char_count[ALPHABET_COUNT] = {0};
pthread_mutex_t count_mutex;
FILE *log_file = NULL; // Global file pointer for logging

typedef struct {
    char **urls;
    int url_count;
    int words_per_batch;
} thread_data_t;

// Function to calculate elapsed time
double get_elapsed_time(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Logging function
void log_results(const char *operation, double elapsed_time) {
    if (log_file) {
        fprintf(log_file, "%s took %.2f seconds\n", operation, elapsed_time);
    } else {
        fprintf(stderr, "Log file not initialized\n");
    }
}

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    char **output = (char **)userp;
    *output = realloc(*output, strlen(*output) + total_size + 1);
    if (!(*output)) {
        fprintf(stderr, "Memory allocation error\n");
        return 0;
    }
    strncat(*output, (char *)contents, total_size);
    return total_size;
}

void process_words(char *text, int words_per_batch) {
    struct timespec process_start, process_end;
    clock_gettime(CLOCK_MONOTONIC, &process_start); // Start timing word processing

    char *word = strtok(text, " \n\r\t");
    int count = 0;
    while (word) {
        char first_char = tolower(word[0]);
        if (isalpha(first_char)) {
            pthread_mutex_lock(&count_mutex);
            word_char_count[first_char - 'a']++;
            pthread_mutex_unlock(&count_mutex);
        }
        count++;
        if (count == words_per_batch) {
            count = 0;
        }
        word = strtok(NULL, " \n\r\t");
    }

    clock_gettime(CLOCK_MONOTONIC, &process_end); // End timing word processing
    double process_time = get_elapsed_time(process_start, process_end);
    log_results("Word processing", process_time);
}

void *thread_task(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Curl initialization failed\n");
        pthread_exit(NULL);
    }

    // Use HTTP/1.1 to avoid HTTP/2 framing issues
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Set timeout to 30 seconds

    for (int i = 0; i < data->url_count; i++) {
        char *response = calloc(1, sizeof(char));
        if (!response) {
            fprintf(stderr, "Memory allocation error\n");
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_URL, data->urls[i]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        int retry_count = 3; // Retry mechanism
        while (retry_count-- > 0) {
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) break;
            fprintf(stderr, "Retrying URL: %s (%s)\n", data->urls[i], curl_easy_strerror(res));
        }

        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to fetch URL: %s (%s)\n", data->urls[i], curl_easy_strerror(res));
            free(response);
            continue;
        }

        process_words(response, data->words_per_batch);
        free(response);
    }

    curl_easy_cleanup(curl);
    pthread_exit(NULL);
}
void generate_graphical_histogram(const char *output_file) {
    int width = 800, height = 600;
    int bar_width = width / ALPHABET_COUNT;
    int max_count = 0;

    // Find the maximum count for scaling
    for (int i = 0; i < ALPHABET_COUNT; i++) {
        if (word_char_count[i] > max_count) {
            max_count = word_char_count[i];
        }
    }

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);

    // Background
    cairo_set_source_rgb(cr, 1, 1, 1); // White
    cairo_paint(cr);

    // Draw bars
    for (int i = 0; i < ALPHABET_COUNT; i++) {
        double bar_height = (double)word_char_count[i] / max_count * (height - 100);
        cairo_set_source_rgb(cr, 0.2, 0.6, 0.8); // Bar color
        cairo_rectangle(cr, i * bar_width, height - bar_height - 50, bar_width - 5, bar_height);
        cairo_fill(cr);

        // Draw labels
        cairo_set_source_rgb(cr, 0, 0, 0); // Black
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);

        char label[2] = { 'A' + i, '\0' };
        cairo_move_to(cr, i * bar_width + bar_width / 4, height - 30);
        cairo_show_text(cr, label);

        // Draw counts
        char count_text[10];
        snprintf(count_text, sizeof(count_text), "%d", word_char_count[i]);
        cairo_move_to(cr, i * bar_width + bar_width / 4, height - bar_height - 60);
        cairo_show_text(cr, count_text);
    }

    // Save to file
    cairo_surface_write_to_png(surface, output_file);

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}



int main(int argc, char *argv[]) {
    struct timespec start, end;

    // Open log file in append mode
    log_file = fopen("execution_log.txt", "a");
    if (!log_file) {
        perror("Error opening log file");
        return EXIT_FAILURE;
    }

    // Write configuration details at the beginning of the log
    fprintf(log_file, "\n=== Execution Log ===\n");
    fprintf(log_file, "Arguments: ");
    for (int i = 0; i < argc; i++) {
        fprintf(log_file, "%s ", argv[i]);
    }
    fprintf(log_file, "\nThreads: %s, Words per Batch: %s\n\n", argv[2], argv[3]);

    clock_gettime(CLOCK_MONOTONIC, &start); // Start timing total execution

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file_path> <num_threads> <words_per_batch>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *file_path = argv[1];
    int num_threads = atoi(argv[2]);
    int words_per_batch = atoi(argv[3]);

    if (num_threads <= 0 || words_per_batch <= 0) {
        fprintf(stderr, "Number of threads and words per batch must be positive integers.\n");
        return EXIT_FAILURE;
    }

    struct timespec file_start, file_end;
    clock_gettime(CLOCK_MONOTONIC, &file_start); // Start timing file reading

    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    char **urls = NULL;
    char line[1024];
    int url_count = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        urls = realloc(urls, (url_count + 1) * sizeof(char *));
        urls[url_count] = strdup(line);
        url_count++;
    }
    fclose(file);

    clock_gettime(CLOCK_MONOTONIC, &file_end); // End timing file reading
    double file_time = get_elapsed_time(file_start, file_end);
    log_results("File reading", file_time);

    if (url_count == 0) {
        fprintf(stderr, "No URLs found in the file\n");
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&count_mutex, NULL);

    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    int urls_per_thread = url_count / num_threads;
    int remaining_urls = url_count % num_threads;

    struct timespec thread_start, thread_end;
    clock_gettime(CLOCK_MONOTONIC, &thread_start); // Start timing thread creation

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].urls = &urls[i * urls_per_thread];
        thread_data[i].url_count = urls_per_thread + (i < remaining_urls ? 1 : 0);
        thread_data[i].words_per_batch = words_per_batch;

        if (pthread_create(&threads[i], NULL, thread_task, &thread_data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &thread_end); // End timing thread creation
    double thread_time = get_elapsed_time(thread_start, thread_end);
    log_results("Thread creation and execution", thread_time);

    for (int i = 0; i < ALPHABET_COUNT; i++) {
        fprintf(log_file, "%c: %d\n", 'A' + i, word_char_count[i]);
    }
// Print histogram after processing is complete
    generate_graphical_histogram("histogram.png");
printf("Graphical histogram saved to 'histogram.png'\n");


    pthread_mutex_destroy(&count_mutex);
    for (int i = 0; i < url_count; i++) {
        free(urls[i]);
    }
    free(urls);

    clock_gettime(CLOCK_MONOTONIC, &end); // End timing total execution
    double elapsed_time = get_elapsed_time(start, end);
    log_results("Total execution", elapsed_time);

    // Close log file
    fclose(log_file);

    return EXIT_SUCCESS;
}
