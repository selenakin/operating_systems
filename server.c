/*
Nurselen Akın
150200087
*/

/*
BLG321E - Homework 3
Multithreaded Web Server
*/

#include "blg312e.h"
#include "request.h"
#include <pthread.h>


int active_connections = 0; 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
sem_t available_slots;
sem_t used_slots;

typedef struct {
    int *connection_queue;
    int id;
} worker_params_t;

void setup_worker_threads(pthread_t *workers, worker_params_t *worker_params, int threads_count, int *connection_queue);
void accept_connections(int listen_fd, int *connection_queue);
void *process_requests(void *params);
int retrieve_connection(int *connection_queue);
void handle_request(int connfd, int thread_id);

void getargs(int *port, int *threads_count, int *queue_size, int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <threads_count> <queue_size>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads_count = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
}

void setup_worker_threads(pthread_t *workers, worker_params_t *worker_params, int threads_count, int *connection_queue) {
    for (int i = 0; i < threads_count; i++) {
        worker_params[i].connection_queue = connection_queue;
        worker_params[i].id = i;
        if (pthread_create(&workers[i], NULL, process_requests, &worker_params[i]) != 0) {
            perror("Failed to create worker thread");
            free(connection_queue);
            free(workers);
            free(worker_params);
            exit(1);
        }
    }
}

void accept_connections(int listen_fd, int *connection_queue) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int connection_fd;
    
    while (1) {
        connection_fd = Accept(listen_fd, (SA *)&client_addr, (socklen_t *)&client_addr_len);

        sem_wait(&available_slots);  // waits for an available slot
        pthread_mutex_lock(&lock);
        connection_queue[active_connections++] = connection_fd;
        pthread_mutex_unlock(&lock);
        sem_post(&used_slots);  // signals that a new connection
    }
}

void *process_requests(void *params) {
    worker_params_t *worker_params = (worker_params_t *)params;
    int *connection_queue = worker_params->connection_queue;
    int id = worker_params->id;

    while (1) {
      int connfd = retrieve_connection(connection_queue);
      handle_request(connfd, id);
    }
    return NULL;
}

int retrieve_connection(int *connection_queue) {
    sem_wait(&used_slots);  // waits for a connection to be available
    pthread_mutex_lock(&lock);
    --active_connections;
    int connfd = connection_queue[active_connections];
    pthread_mutex_unlock(&lock);
    sem_post(&available_slots); // signals that a slot is now available
    return connfd;
}

void handle_request(int connfd, int thread_id) {
    printf("Thread %d is processing connection number %d\n", thread_id, connfd);
    requestHandle(connfd);
    close(connfd);
}

int main(int argc, char *argv[]) {
    int listen_fd, port, threads_count, queue_size, *connection_queue;

    getargs(&port, &threads_count, &queue_size, argc, argv);
    connection_queue = (int *)malloc(sizeof(int) * queue_size);
    if (connection_queue == NULL) {
        perror("Failed to allocate memory for connection queue");
        exit(1);
    }

    // initializing semaphores 
    sem_init(&available_slots, 0, queue_size);  
    sem_init(&used_slots, 0, 0);  

    // creating worker threads
    pthread_t *workers = (pthread_t *)malloc(sizeof(pthread_t) * threads_count);
    worker_params_t *worker_params = (worker_params_t *)malloc(sizeof(worker_params_t) * threads_count);
    setup_worker_threads(workers, worker_params, threads_count, connection_queue);

    listen_fd = Open_listenfd(port);
    accept_connections(listen_fd, connection_queue);

    // cleanup
    free(connection_queue);
    free(workers);
    free(worker_params);
    sem_destroy(&available_slots);
    sem_destroy(&used_slots);
    return 0;
}