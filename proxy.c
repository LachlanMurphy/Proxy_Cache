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

#define BUFFERSIZE 2048
#define INDEX "index.html"
#define ROOT_DIR "./www"
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
    socket_arg_t* args = (socket_arg_t *) arg;
    char buf[BUFFERSIZE];
    bzero(buf, BUFFERSIZE);

    // read in message
    if (read(args->clientfd, buf, BUFFERSIZE) < 0) error("ERROR in reading from socket");
    printf("%s", buf);
    
    // parse message
    char req[5][BUFFERSIZE / 2]; // req[0]=method ; req[1]=URI ; req[2]=version ; req[3]=hostname ; req[4]=body
    char* tmp;
    int parse_err = 0;
    for (int i = 0; i < 3; i++) {
        if (i == 0) tmp = strtok(buf, " ");
        else tmp = strtok(NULL, " ");

        if (!tmp) {
            parse_err = 1;
            break;
        }
        memcpy(req[i], tmp, BUFFERSIZE);
    }

    // version may have a cairraige return on the end, replace with str terminator
    // also get the host name and port number
    int line_brk = strcspn(req[2], "\n");
    if (!parse_err) {
        req[2][line_brk] = '\0';
        // 6 is the number of characters in the hostline that is not the actual host name
        // eg: Host: www.hostname.com
        char* host_line = buf + 9 + (line_brk + strlen(req[0]) + strlen(req[1])) * sizeof(char);
        strncpy(req[3], host_line, strcspn(host_line, "\r"));
        req[3][strcspn(req[3], "\r")] = '\0';
        char* body_ptr = strstr(buf, "\r\n\r\n")+4;

        // TODO: seg fault happening with strcspn
        if (strcspn(body_ptr, "\0") == 0) {
            req[4][0] = '\0';
        } else {
            strncpy(req[4], body_ptr, strcspn(body_ptr, "\0"));
        }
        printf("hostname: %s ; %s\n", req[3], req[4]);
        // printf("String in hex: ");
        // for (size_t i = 0; i < strlen(host_line); i++) {
        //     printf("%02x ", (unsigned char)host_line[i]);
        // }
        // printf("\n");
    }
    exit(0);


    if (parse_err) {
        // 400 Bad request
        sprintf(buf, HEADER, "HTTP/1.1", "400 Bad Request", "text/plain", 0UL);
        if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
    } else if (strncmp(req[0], "GET", strlen("GET"))) { // Check method, only get is allowed
        // 405 Method Not Allowed
        sprintf(buf, HEADER, req[2], "405 Method Not Allowed", "text/plain", 0UL);
        if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
    } else if (strncmp("HTTP/1.0", req[2], strlen(req[2]) - 1) &&
            strncmp("HTTP/1.1", req[2], strlen(req[2]) - 1)) { // check HTTP version
        // 505 HTTP Version Not Supported
        sprintf(buf, HEADER, req[2], "505 HTTP Version Not Supported", "text/plain", 0UL);
        if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
    } else {
        char file_name[BUFFERSIZE];
        strncpy(file_name, ROOT_DIR, strlen(ROOT_DIR)+1);
        strcat(file_name, req[1]);

        // if we are requesting a directory, replace ending with index.html
        if (file_name[strlen(file_name)-1] == '/') {
            // char indx_suffix
            strcat(file_name, (char *) &INDEX);
        }

        // attempt to access file requested
        if (!access(file_name, F_OK) && !access(file_name, R_OK)) {
            // file exists

            // send file
            FILE* file = fopen(file_name, "r");

            // get file size
            fseek(file, 0, SEEK_END);
            unsigned long file_size = ftell(file);
            fseek(file, 0, SEEK_SET);

            // send the file header
            sprintf(buf, HEADER, req[2], "200 OK", file_types[find_file_type(file_name)], file_size);
            if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");

            // send file content
            off_t offset = 0;
            int n = 0;
            while (n < file_size) {
                n = sendfile(args->clientfd, fileno(file), &offset, file_size);
                if (n < 0) error("ERROR in sendfile");
            }
            fclose(file);
        } else {
            // check whether it was a permission err or file doesn't exist
            if (access(file_name, F_OK)) {
                // 404 file not found
                sprintf(buf, HEADER, req[2], "404 Not Found", "text/plain", 0UL);
                if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
            } else if (access(file_name, R_OK)) {
                // 403 forbidden
                sprintf(buf, HEADER, req[2], "403 Forbidden", "text/plain", 0UL);
                if (send(args->clientfd, buf, strlen(buf), 0) < 0) error("ERROR in send");
            }
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