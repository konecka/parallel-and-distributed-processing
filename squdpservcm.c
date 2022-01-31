#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 

#include "sqserv.h"

char *prog_name;

void usage() {
    printf("usage: %s [-p <port>]\n", prog_name);
    printf("\t-p <port> specify alternate port\n");
}

int main(int argc, char* argv[]) {

    long port = DEFAULT_PORT;
    struct sockaddr_in sin;
    int fd, new_socket, master_socket , client_socket[30];
    socklen_t sin_len;
    size_t  data_len;
    ssize_t len;
    fd_set readfds; 
    char *data, *end;
    long ch;
    char sendBuff[1025];
    int opt = 1;
    int max_clients = 3;

    int max_sd, sd, activity; 

    /* get options and arguments */
    prog_name = strdup(basename(argv[0]));
    while ((ch = getopt(argc, argv, "?hp:")) != -1) {
        switch (ch) {
            case 'p': port = strtol(optarg, (char **)NULL, 10);break;
            case 'h':
            case '?':
            default: usage(); return 0;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
        return EX_USAGE;
    }

    /* get room for data */
    data_len = BUFFER_SIZE;
    if ((data = (char *) malloc(data_len)) < 0) {
        err(EX_SOFTWARE, "in malloc");
    }

    /* create and bind a socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        free(data);
        err(EX_SOFTWARE, "in socket");
    }
    memset(&sin, 0, sizeof(sin));
    memset(sendBuff, '0', sizeof(sendBuff));
    
    // attaching socket 
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY; // rather memcpy to sin.sin_addr
    sin.sin_port = htons(port);


    for (int i = 0; i < max_clients; i++)  
    {  
        client_socket[i] = 0;  
    }  
         
    //create a master socket 
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)  
    {  
        perror("socket failed");  
        exit(EXIT_FAILURE);  
    }  
     
    if (bind(master_socket, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        free(data);
        close(fd);
        err(EX_SOFTWARE, "in bind");
    }

    /* accept connection */
    // listen(fd, 10);
    if (listen(master_socket, 3) < 0)  
    {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    } 

    // puts("Listen...");  
    printf("listening on port %d\n", (int)port);

    int valread;
    char buffer[1024] = {0};

    /* accept connection */
    // new_socket = accept(fd, (struct sockaddr *)&sin, &sin_len);

    while (1) {  
        //clear the socket set 
        FD_ZERO(&readfds);  
     
        //add master socket to set 
        FD_SET(master_socket, &readfds);  
        max_sd = master_socket;  
             
        //add child sockets to set 
        for (int i = 0; i < max_clients; i++)  
        {  
            sd = client_socket[i];  
            if(sd > 0)  
                FD_SET( sd , &readfds);  
            if(sd > max_sd)  
                max_sd = sd;  
        }  
     
        //wait for an activity
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);  
       
        if ((activity < 0) && (errno!=EINTR))  
        {  
            printf("select error");  
        } 

        /* accept connection */
        if (FD_ISSET(master_socket, &readfds)) {  
            if ((new_socket = accept(master_socket, 
                    (struct sockaddr *)&sin, (socklen_t*)&sin_len))<0)  
            {  
                perror("accept");  
                exit(EXIT_FAILURE);  
            }  
             
            printf("New connection\n");  
           
            //add new socket to array of sockets 
            for (int i = 0; i < max_clients; i++)  
            {  
                if( client_socket[i] == 0 )  
                {  
                    client_socket[i] = new_socket;    
                    break;  
                }  
            }
            while(1) {
                valread = read(new_socket , buffer, 1024);

                ch = strtol(buffer, &end, 10);
                if (ch != 0) {
                    printf("num: %ld\n", ch);
                    switch (errno) {
                        case EINVAL: err(EX_DATAERR, "not an integer");
                        case ERANGE: err(EX_DATAERR, "out of range");
                        default: if (ch == 0 && data == end) errx(EX_DATAERR, "no value");  // Linux returns 0 if no numerical value was given
                    }

                    /* calculate the square */
                    ch *= ch;
                    int length = snprintf( NULL, 0, "%ld", ch);
                    char res[length];
                    snprintf(res, length + 1, "%ld", ch);

                    send(new_socket , res , strlen(res) , 0);
                    printf("answer sent: %s\n", res);
                    
                    /* cleanup */
                    memset(&(res[0]), 0, 20);
                    sleep(1);
                }
            }
        }  
    }

    /* cleanup */
    free(data);
    close(fd);

    return EX_OK;
}
