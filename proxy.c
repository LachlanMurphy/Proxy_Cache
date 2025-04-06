/*
* Basic TCP Web Server
* Lachlan Murphy
* 21 February 2025
*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include "array.h"

#define BUFFERSIZE 1024
#define CAHCE_DIR "./cache"
// HTTP version, status, content type, content length
#define HEADER "%s %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n\r\n"
#define FILE_SUF_LEN 8

// argument struct for socket_handler function
typedef struct {
    int serverfd;
    int clientfd;
    struct sockaddr_in* serveraddr;
    socklen_t* addrlen;
    array* arr;
    pthread_t* thread_id;
} socket_arg_t;

// multi-thread function to handle new socket connections
void* socket_handler(void* arg);

// matches a file suffix with a file type
int find_file_type(char* file_name);

/*
* error - wrapper for perror
*/
void error(char *msg) {
    perror(msg);
    exit(1);
}

// sigint handler
void sigint_handler(int sig);

// global values
array socks; // semaphores used, thread safe
const char* file_types[FILE_SUF_LEN] = {
    "text/html",
    "text/plain",
    "image/png",
    "image/gif",
    "image/jpg",
    "image/x-icon",
    "text/css",
    "application/javascript"
}; // read only, thread safe

// indices align with file_types
const char* file_suff[FILE_SUF_LEN] = {
    ".html",
    ".txt",
    ".png",
    ".gif",
    ".jpg",
    ".ico",
    ".css",
    ".js"
}; // read only, thread safe

int main(int argc, char** argv) {
    int sockfd, new_socket;
    int portno, timeout;
    int optval;
    struct sockaddr_in serveraddr;
    socklen_t addrlen = sizeof(serveraddr);

    /* 
    * check command line arguments
    */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <timeout length>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);
    timeout = atoi(argv[2]);

    // set up signal handling
    signal(SIGINT, sigint_handler);

    // initialize shared array
    array_init(&socks);
    
    // socket: create the parent socket 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");

    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    
    // build the server's Internet address
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    // bind: associate the parent socket with a port 
    if (bind(sockfd, (struct sockaddr *) &serveraddr, addrlen) < 0) error("ERROR on binding");

    // main loop, listen for sockets and deal with them
    while (1) {
        // wait for new connection to be prompted
        if (listen(sockfd, 5) < 0) error("ERROR on listen");
        
        // create new fd for that socket
        if ((new_socket = accept(sockfd, (struct sockaddr *) &serveraddr, &addrlen)) < 0) error("ERROR accepting new socket");

        // create new pthread to handle socket request
        // this pointer will eventually be removed from scopre without freeing the memmory
        // the memmory will be freed when the socket is successfully completed
        pthread_t* thread_id = malloc(sizeof(pthread_t));
        
        // init arguments for new thread
        socket_arg_t* socket_arg = malloc(sizeof(socket_arg_t)); // will also be freed later
        socket_arg->addrlen = &addrlen;
        socket_arg->clientfd = new_socket;
        socket_arg->serveraddr = &serveraddr;
        socket_arg->serverfd = sockfd;
        socket_arg->arr = &socks;

        // create thread
        pthread_create(thread_id, NULL, socket_handler, socket_arg);

        // update args
        socket_arg->thread_id = thread_id;

        // add new thread to array
        array_put(&socks, thread_id);

        // detatch thread so resources are unallocated independent of parent thread
        pthread_detach(*thread_id);
    }
}

void sigint_handler(int sig) {
    print_array(&socks);
    // wait for all sockets to finish computing
    while (socks.size);
    array_free(&socks);
    printf("Server closed on SIGINT\n");
    exit(0);
}

void* socket_handler(void* arg) {
    int bytes_read, port;
    int server_socket;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    socket_arg_t* args = (socket_arg_t *) arg;
    char buf[BUFFERSIZE];
    char token_buf[BUFFERSIZE+8];
    char* token;
    bzero(buf, BUFFERSIZE);
    bzero(token_buf, BUFFERSIZE);

    // read in message
    if ((bytes_read = read(args->clientfd, buf, BUFFERSIZE)) < 0) error("ERROR in reading from socket");
    buf[bytes_read] = '\0';
    printf("%s", buf);
    
    strncpy(token_buf, buf, BUFFERSIZE);
    
    // parse message
    char req[4][BUFFERSIZE / 2]; // req[0]=method ; req[1]=URI ; req[2]=version ; req[3]=hostname
    int parse_err = 0;
    for (int i = 0; i < 3; i++) {
        if (i == 0) token = strtok(token_buf, " ");
        else token = strtok(NULL, " ");

        if (!token) {
            parse_err = 1;
            break;
        }
        memcpy(req[i], token, BUFFERSIZE);
    }

    // version may have a cairraige return on the end, replace with str terminator
    // also get the host name and port number
    if (!parse_err) {
        req[2][strcspn(req[2], "\r")] = '\0';


        strncpy(token_buf, buf, BUFFERSIZE);
        token = strtok(token_buf, "\r\n");
        while (token) {
            if (!strncmp(token, "Host: ", strlen("Host: "))) {
                strncpy(req[3], token+strlen("Host: "), BUFFERSIZE);
                break;
            } else {
                token = strtok(NULL, "\r\n");
            }
        }

        if (!token) parse_err = 1;

        // get port number
        int colon = strcspn(req[3], ":");
        if (colon != strlen(req[3])) {
            port = atoi(req[3]+colon+1);
        } else {
            port = 80;
        }
        req[3][colon] = '\0';

        // now that we have the host, we can extract the URI
        char* path = strcspn(req[1], "http://")+req[1]+strlen("http://");
        path += strcspn(path, "/");
        int len = strlen(path);
        strncpy(req[1], path, strlen(path));
        req[1][len] = '\0';

    }
    


    if (parse_err || strncmp(req[0], "GET", strlen("GET"))) {
        // 400 Bad request
        sprintf(buf, HEADER, "HTTP/1.1", "400 Bad Request", "text/plain", 0UL);
        if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
    } else {
        // connect to server and request data
        if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("server socket");
        
        /* gethostbyname: get the server's DNS entry */
        server = gethostbyname(req[3]);
        if (server == NULL) {
            fprintf(stderr,"ERROR, no such host as %s\n", req[3]);
            exit(0);
        }

        /* build the server's Internet address */
        bzero((char *) &serveraddr, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
        serveraddr.sin_port = htons(port);

        if (connect(server_socket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) error("connecting to server");

        // make new HTTP request
        char* headers = strcspn(buf, "\r\n")+2+buf;
        sprintf(token_buf, "GET %s %s\r\n", req[1], req[2]);
        // sprintf(token_buf, "GET %s %s\r\n", req[1], req[2]);
        strcat(token_buf, headers);

        if (send(server_socket, token_buf, BUFFERSIZE, 0) < 0) error("ERROR in send");

        int n;
        // send initial meta data
        if ((n = recv(server_socket, buf, BUFFERSIZE, 0)) < 0) error("ERROR in recv");
        if (send(args->clientfd, buf, n, 0) < 0) error("ERROR in send");
        
        // get content length
        int content_length = 0;
        strncpy(token_buf, buf, BUFFERSIZE);
        token = strtok(token_buf, "\r\n");
        while (token) {
            if (!strncmp(token, "Content-Length: ", strlen("Content-Length: "))) {
                content_length = atoi(token+strlen("Content-Length: "));
                break;
            } else {
                token = strtok(NULL, "\r\n");
            }
        }

        int content_rec = 0;
        while (content_rec < content_length) {
            
            if ((n = recv(server_socket, buf, BUFFERSIZE, 0)) < 0) error("ERROR in recv");
            content_rec += n;
            if (n == 0) break;

            // send response back to client
            if (send(args->clientfd, buf, n, 0) < 0) error("ERROR in send1");
        }
    }

    // socket no longer needed
    close(args->clientfd);

    // remove current thread from global array
    array_get(args->arr, args->thread_id);

    // free memory alocated by malloc from main thread
    free(args->thread_id);
    free(args);
    return NULL;
}

int find_file_type(char* file_name) {
    // get suffix of file name
    char* suf = strrchr(file_name, '.');
    if (!suf || suf == file_name) return 1; // by default do plain text

    // match with suffixes from file_suf array
    for (int i = 0; i < FILE_SUF_LEN; i++) {
        if (strncmp(suf, file_suff[i], strlen(suf)) == 0) return i;
    }

    return 1; // by default just do plain text
}