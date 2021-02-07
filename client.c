#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

int check(void *buf);
int valid_pattern(void *buf);

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int check(void *buf){ // comparing between the buffers to see if its equal then its legit command
    int a = strncmp("Login " , buf , 6);
    int b = strncmp("ReadGrade" , buf , 9);
    int c = strncmp("UpdateGrade " , buf , 11);
    int d = strncmp("Logout" , buf , 6);
    int e = strncmp("GradeList" , buf , 9);
    int f = strncmp("Exit" , buf , 4);
    if (a == 0 || c == 0){ // becase Login and UpdateGrade has the same pattern which include command [id] [grade]/[password] we can check if it exists.
        if (valid_pattern(buf) == 1){
            return 1;
        } else {
            return 0;
        }
    } else {
        if (b == 0 || d == 0 || e == 0 || f == 0){
            return 1;
        } else {
            return 0;
            }
        }
}
int valid_pattern(void *buf){
    int counter = 0;
    char * token = strtok(buf, " "); // loop through the string to extract all other tokens
    while( token != NULL ) {
        counter++;
        token = strtok(NULL, " ");
    }
    if(counter == 3){
        return 1;
    } else {
        return 0;
    }

}
void start(char *argv[]){

    int sockfd, portno, n;
    int fd[2];
    int fd2[2];
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (pipe(fd) == -1){ // creating an pipe
        fprintf(stderr,"An error ocured with opening the pipe\n");
        exit(0);
    }
    if (pipe(fd2) == -1){ // creating an pipe
        fprintf(stderr,"An error ocured with opening the pipe\n");
        exit(0);
    }

    portno = atoi(argv[2]); // turn port number into int from string
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // open a socket
    if (sockfd < 0) // checking if the socket is valid
        error("ERROR opening socket");
    server = gethostbyname(argv[1]); // checking if the serrver name is valid
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");


    if (fork() != 0){ //Son process
        close(fd[1]);  // closing File descriptor of writing because he only reads
        close(fd2[0]); //close file descriptor 2 for reading
        while(1){
            int valid = 1;
            bzero(buffer,256);
            while (valid){
                read(fd[0],buffer,255); // reading from pipe
                if (strcmp(buffer,"/000/") == 0){
                    bzero(buffer,256);
                } else {
                    valid = 0;
                }
            }
            valid = 1;
            int i = strncmp("Exit" , buffer , 4); // checking if the command is Exit or Logout because afterthat we need to close the socket.
            int j = strncmp("GradeList",buffer, 9);
            if (j != 0){
                n = write(sockfd,buffer,strlen(buffer)+1); // writing to socket server
                if (n < 0)
                     error("ERROR writing to socket");
                bzero(buffer,256);
                if (i != 0){
                    n = read(sockfd,buffer,255); // reading from server
                    if (n < 0)
                        error("ERROR reading from socket");
                    }
            } else {
                unsigned int number_of_bytes = 0;
                n = write(sockfd,buffer,strlen(buffer)+1);
                if (n < 0)
                     error("ERROR writing to socket");
                bzero(buffer,256);
                n = read(sockfd,&number_of_bytes,sizeof(number_of_bytes)); // reading from server the number of bytes to read
                if (n < 0)
                     error("ERROR reading from socket");
                bzero(buffer,256);
                number_of_bytes = ntohl(number_of_bytes);
                char *temp= malloc(number_of_bytes);
                n = read(sockfd,temp,number_of_bytes);
                printf("%s",temp);
                free(temp);
            }
            if ( i != 0){
                printf("%s",buffer);
                write(fd2[1],buffer,strlen(buffer)+1);
            }else {
                close(fd[0]);
                close(fd2[1]);
                close(sockfd);
                exit(0);
                }
            }
    } else {
        char test[256];
        close(fd[0]);
        close(fd2[1]); // close fd2 for writind
        while(1)
        {
            printf("> ");
            bzero(buffer,256);
            bzero(test,256);
            strtok(fgets(buffer,255,stdin),"\n");
            strcpy(test,buffer);
            if (check(test)){ // using check funtion to see if the string is a legit command
                bzero(test,256);
                write(fd[1],buffer,strlen(buffer)+1);
                if (strncmp("Exit" , buffer , 4) == 0){
                    close(sockfd);
					exit(0);
                    }
                read(fd2[0],test,255);
            } else{
                printf("Wrong Input\n");
            }
        }
    }
}


int main(int argc, char *argv[])
{
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    } else {
        start(argv);
    }
    return 0;
}
