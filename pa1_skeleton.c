/*
# Copyright 2025 University of Kentucky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

/* 
Please specify the group members here

# Student #1: Matthew Hendrix
# Student #2: Mason Wooldridge

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

/*
 * This structure is used to store per-thread data in the client
 */
typedef struct {
    int epoll_fd;        /* File descriptor for the epoll instance, used for monitoring events on the socket. */
    int socket_fd;       /* File descriptor for the client socket connected to the server. */
    long long total_rtt; /* Accumulated Round-Trip Time (RTT) for all messages sent and received (in microseconds). */
    long total_messages; /* Total number of messages sent and received. */
    float request_rate;  /* Computed request rate (requests per second) based on RTT and total messages. */
} client_thread_data_t;

/*
 * Helper method: Set file descriptor to non-blocking mode.
 */
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) {
        return -1;
    }
    
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/*
 * This function runs in a separate client thread to handle communication with the server
 */
void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP"; /* Send 16-Bytes message every time */
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end;

    // Hint 1: register the "connected" client_thread's socket in the its epoll instance
    // Hint 2: use gettimeofday() and "struct timeval start, end" to record timestamp, which can be used to calculated RTT.

    /* TODO:
     * It sends messages to the server, waits for a response using epoll,
     * and measures the round-trip time (RTT) of this request-response.
     */
 
    /* TODO:
     * The function exits after sending and receiving a predefined number of messages (num_requests). 
     * It calculates the request rate based on total messages and RTT
     */

    int ret;
    
    event.events = EPOLLIN;
    event.data.fd = data -> socket_fd;
    
    if(epoll_ctl(data -> epoll_fd, EPOLL_CTL_ADD, data -> socket_fd, &event) < 0) {
        perror("client epoll_ctl");
        pthread_exit(NULL);
    }
    
    data -> total_rtt = 0;
    data -> total_messages = 0;
    
    for (int i = 0; i < num_requests; i++) {
        if(gettimeofday(&start, NULL) < 0) {
            perror("gettimeofday");
            break;
        }
        
        ret = send(data -> socket_fd, send_buf, MESSAGE_SIZE, 0);
        
        if(ret < 0) {
            perror("send");
            break;
        }
        
        int nfds = epoll_wait(data -> epoll_fd, events, MAX_EVENTS, 5000);
        
        if(nfds <= 0) {
            perror("client epoll_wait");
            break;
        }
        
        ret = recv(data -> socket_fd, recv_buf, MESSAGE_SIZE, 0);
        
        if(ret <= 0) {
            perror("recv");
            break;
        }
        
        if(gettimeofday(&end, NULL) < 0) {
            perror("gettimeofday");
            break;
        }
        
        long long rtt = (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_usec - start.tv_usec);
        data -> total_rtt += rtt;
        data -> total_messages++;
    }
    
    if(data -> total_rtt > 0) {
        data -> request_rate = (data -> total_messages * 1000000.0) / data -> total_rtt;
    } else {
        data -> request_rate = 0;
    }
    
    pthread_exit(NULL);
    
    return NULL;
}

/*
 * This function orchestrates multiple client threads to send requests to a server,
 * collect performance data of each threads, and compute aggregated metrics of all threads.
 */
void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    /* TODO:
     * Create sockets and epoll instances for client threads
     * and connect these sockets of client threads to the server
     */

    int i, ret;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    // Hint: use thread_data to save the created socket and epoll instance for each thread
    // You will pass the thread_data to pthread_create() as below
    for (int i = 0; i < num_client_threads; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        
        if(sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        
        if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect");
            exit(EXIT_FAILURE);
        }
        
        int epfd = epoll_create1(0);
        
        if(epfd < 0) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }
        
        thread_data[i].socket_fd = sockfd;
        thread_data[i].epoll_fd = epfd;
        thread_data[i].total_rtt = 0;
        thread_data[i].total_messages = 0;
        thread_data[i].request_rate = 0.0;
        
        ret = pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);

        if(ret != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    /* TODO:
     * Wait for client threads to complete and aggregate metrics of all client threads
     */

    long long total_rtt = 0;
    long total_messages = 0;
    float total_request_rate = 0.0;
    
    for(i = 0; i < num_client_threads; i++){
        pthread_join(threads[i], NULL);
        total_rtt += thread_data[i].total_rtt;
        total_messages += thread_data[i].total_messages;
        total_request_rate += thread_data[i].request_rate;
        
        close(thread_data[i].socket_fd);
        close(thread_data[i].epoll_fd);
    }
    
    printf("Average RTT: %lld us\n", total_rtt / total_messages);
    printf("Total Request Rate: %f messages/s\n", total_request_rate);
}

void run_server() {
    /* TODO:
     * Server creates listening socket and epoll instance.
     * Server registers the listening socket to epoll
     */
    
    int listen_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct epoll_event ev, events[MAX_EVENTS];
    char buffer[MESSAGE_SIZE];
    
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    if(set_nonblocking(listen_fd) < 0) {
        perror("set_nonblocking");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);
    
    if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    if(listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    epoll_fd = epoll_create1(0);
    
    if(epoll_fd < 0) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl: listen_fd");
        exit(EXIT_FAILURE);
    }
    
    printf("Server is listening on %s:%d\n", server_ip, server_port);

    /* Server's run-to-completion event loop */
    while (1) {
        /* TODO:
         * Server uses epoll to handle connection establishment with clients
         * or receive the message from clients and echo the message back
         */
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if(nfds < 0) {
            perror("epoll_wait");
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
                
                if(client_fd < 0) {
                    if(errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept");
                    }
                    
                    continue;
                }
                
                if(set_nonblocking(client_fd) < 0) {
                    perror("set_nonblocking for client");
                    close(client_fd);
                    continue;
                }
                
                struct epoll_event client_ev;
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;
                
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                }
            } else {
                int client_fd = events[i].data.fd;
                ssize_t nread = read(client_fd, buffer, MESSAGE_SIZE);
                
                if(nread <= 0) {
                    if(nread < 0) {
                        perror("read");
                    }
                    
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                } else {
                    ssize_t nsent = write(client_fd, buffer, nread);
                    
                    if(nsent < 0) {
                        perror("write");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                    }
                }
            }
        }
    }
    
    close(listen_fd);
    close(epoll_fd);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "server") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);

        run_server();
    } else if (argc > 1 && strcmp(argv[1], "client") == 0) {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);

        run_client();
    } else {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
    }

    return 0;
}
