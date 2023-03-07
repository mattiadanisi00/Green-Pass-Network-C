//
// Created by Mattia Danisi on 17/02/23.
//

#include "ServerV.h"

volatile sig_atomic_t running = 1;

void *handle_connection(void *);
void sigint_handler(int);
int write_gp_to_file(FILE *, struct green_pass);
int search_gp_in_file(FILE *, struct green_pass);
int update_validity_in_file(FILE *, struct green_pass);

pthread_mutex_t mutex;


int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len;
    pthread_t thread_pool[THREAD_POOL_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);


    pthread_mutex_init(&mutex, NULL);

    // create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(1);
    }


    // bind server socket to a port
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all available network interfaces
    server_address.sin_port = htons(SERVER_V_PORT); // listen on port 1024
    if (bind(server_socket, (SA *) &server_address, sizeof(server_address)) == -1) {
        perror("bind");
        exit(2);
    }

    // listen for connections
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("listen");
        exit(3);
    }

    printf("server V listening on port %d\n", SERVER_V_PORT);

    // initialize thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // wait for connections and assign to threads in pool
    while (running) {
        client_address_len = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        // find first available thread in pool
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                // thread is available
                thread_index = i;
                break;
            }
        }

        if (thread_index == -1) {
            // no threads available, close connection
            close(client_socket);
        } else {
            // pass client socket to thread
            int *client_socket_ptr = malloc(sizeof(int));
            *client_socket_ptr = client_socket;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_socket_ptr) != 0) {
                perror("pthread_create");
                break;
            } else {
                // detach thread so it can run in the background
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    perror("pthread_detach");
                    break;
                }
            }
        }
    }

    pthread_mutex_destroy(&mutex);
    close(client_socket);
    close(server_socket);
    return 0;
}


void *handle_connection(void *arg) {
    int client_socket;
    if (arg == NULL) {
        // handle error
        pthread_exit(NULL);
    } else {
        client_socket = *((int *) arg);
        free(arg);
    }

    // Receive the struct from the client
    struct green_pass gp;

    int bytes_received = recv(client_socket, &gp, sizeof(gp), 0);
    if (bytes_received == -1) {
        perror("recv");
        close(client_socket);
        pthread_exit(NULL);
    } else if (bytes_received != sizeof(gp)) {
        close(client_socket);
        pthread_exit(NULL);
    }

    //lock mutex
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("pthread_mutex_lock");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Open data file
    FILE *fp = fopen(FILE_NAME, "rb+");
    if (fp == NULL) {
        perror("fopen");
        close(client_socket);
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    //change the validity of a green pass
    if (gp.service == SERVICE_VALIDITY_GP) {
        printf("service requested : validity\n");

        // Find the corresponding code in the file and change its validity
        int response = update_validity_in_file(fp, gp);

        if(response) {
            printf("green pass number %s 's validity successfully updated\n",gp.card);
            if (send(client_socket, "SUCCESS", 7, 0) == -1) {
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        } else {
            printf("green pass number %s 's validity update failed\n",gp.card);
            if (send(client_socket, "FAIL", 4, 0) == -1) {
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        }
    }

    //service : 1 - search the code in the file
    else if (gp.service == SERVICE_READ_GP) {

        printf("service requested : read\n");

        int response = search_gp_in_file(fp, gp);

        if (response == EXPIRED) {
            printf("green pass number %s is expired\n",gp.card);
            if (send(client_socket, "EXPIRED", 7, 0) == -1) {
                perror("send");
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        } else if (response == VALID) {
            printf("green pass number %s is valid\n",gp.card);
            if (send(client_socket, "VALID", 5, 0) == -1) {
                perror("send");
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        } else if (response == NOT_VALID) {
            printf("green pass number %s is not valid\n",gp.card);
            if (send(client_socket, "NOT VALID", 9, 0) == -1) {
                perror("send");
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        } else {
            printf("green pass number %s not found\n",gp.card);
            if (send(client_socket, "NOT FOUND", 9, 0) == -1) {
                perror("send");
                fflush(fp);
                fclose(fp);
                close(client_socket);
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
        }
    }

    //service : 0 - write the code in the file
    else {
        printf("service requested : write\n");
        int response = write_gp_to_file(fp, gp);

        if (send(client_socket, &response, sizeof(int), 0) == -1) {
            fflush(fp);
            fclose(fp);
            close(client_socket);
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }
    }

    fflush(fp);
    fclose(fp);
    close(client_socket);
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

void sigint_handler(int sig) {
    running = 0;
}

int write_gp_to_file(FILE *fp, struct green_pass gp) {

    // Check if the code already exists in the file
    rewind(fp);
    char line[TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 1];
    while (fgets(line, TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 1, fp) != NULL) {
        char code[TESSERA_SANITARIA_LEN + 1];
        int n_items = sscanf(line, "%[^:/]:", code);
        if (n_items != 1) {
            continue; // Invalid line, skip it
        }

        //given code already exists
        if (strcmp(code, gp.card) == 0) {
            printf("given code already exists\n");
            fflush(fp);
            fclose(fp);
            return 0;
        }
    }

    // Given code does not exist
    // Compute the actual expiry date based on the validity time left
    time_t now = time(NULL);
    time_t expiry_time = now + gp.expiration_date;
    struct tm *tm_struct = localtime(&expiry_time);

    // Write the code and expiry date to the end of the file
    fseek(fp, 0, SEEK_END);
    fprintf(fp, "%s:%d/%d/%d:1\n", gp.card, tm_struct->tm_mday, tm_struct->tm_mon + 1, tm_struct->tm_year + 1900);
    fflush(fp);
    return 1;
}
int search_gp_in_file(FILE *fp, struct green_pass gp) {
    char line[TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 3];
    while (fgets(line, TESSERA_SANITARIA_LEN + EXPIRATION_DATE_LEN + 3, fp) != NULL) {

        // Extract the code and expiration date from the line
        char code[TESSERA_SANITARIA_LEN + 1];
        int day, month, year;
        int n_items = sscanf(line, "%[^:/]:%d/%d/%d", code, &day, &month, &year);
        if (n_items != 4) {
            continue;  // Invalid line, skip it
        }

        if (strcmp(code, gp.card) == 0) {
            printf("Code found\n");
            // Code found, check if it's still valid
            char *last_char = line + strlen(line) - 1;
            if (*last_char == '1') {
                time_t now = time(NULL);
                struct tm *tm_struct = localtime(&now);
                tm_struct->tm_mday = day;
                tm_struct->tm_mon = month - 1;
                tm_struct->tm_year = year - 1900;
                time_t expiry_time = mktime(tm_struct);

                if (expiry_time < now) {
                    // Code expired, send response to client
                    return EXPIRED;
                } else {
                    // Code still valid, send response to client
                    return VALID;
                }
            } else {
                //code not valid
                return NOT_VALID;
            }
        }
    }

    return NOT_FOUND;
}
int update_validity_in_file(FILE *fp, struct green_pass gp) {
    fseek(fp, 0, SEEK_SET);
    char line[100];
    long int offset = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, gp.card, TESSERA_SANITARIA_LEN) == 0) {
            // Found the corresponding code
            int validity = line[strlen(line) - 2] - '0';
            int new_validity = validity == 1 ? 0 : 1;
            offset = -2;
            fseek(fp, offset, SEEK_CUR);
            fprintf(fp, "%d\n", new_validity);
            fflush(fp);
            return TRUE;
        }
    }

    return FALSE;
}
