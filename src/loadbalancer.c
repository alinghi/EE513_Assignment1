/*
    C socket server example, handles multiple clients using threads
    http://www.binarytides.com/server-client-example-c-sockets-linux/
*/
 
#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <sys/poll.h> 
#include <stdbool.h>


static int clientFD[2];
static int handlerFD[3];
static int numberOfConnect=0;
FILE * fp;



//connect with client
bool connectWithClient(){
    /*-------------------------------------------------------
    Connect with Client(2 client)
    -------------------------------------------------------*/
    int socket_desc,c;
    struct sockaddr_in server,client_addr;
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
        exit(0);
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    memset(&server,0,sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 5131 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        exit(0);
    }
    puts("bind done");
    c = sizeof(struct sockaddr_in); 
    //Listen
    listen(socket_desc , 2);
    for(int i=0;i<2;i++){
        clientFD[i]=accept(socket_desc, (struct sockaddr *)&client_addr, (socklen_t*)&c);        
    }
    return true;
}

//connect with handler
bool connectWithHandler(){
    /*-------------------------------------------------------
    Connect with Handler(3 handler)
    -------------------------------------------------------*/
    int socket_desc,c;
    struct sockaddr_in server,client_addr;
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
        exit(0);
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    memset(&server,0,sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 9000 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        exit(0);
    }
    puts("bind done");
    c = sizeof(struct sockaddr_in); 
     
    //Listen
    listen(socket_desc , 2);
    for(int i=0;i<3;i++){
        handlerFD[i]=accept(socket_desc, (struct sockaddr *)&client_addr, (socklen_t*)&c);        
    }
    return true;
}


//Data Format
struct dataFormat{
    //field that can be set at init time

    uint32_t clientID;
    uint32_t transactionID;

    //put: 1, put-ack: 2, get: 3, get-ack: 4, del: 5, del-ack: 6
    uint16_t cmd;

    //none*: 0, success: 1, not exist: 2, already exist: 3
    //* Only used for non-ack messages
    uint16_t code;
    uint16_t keyLength;
    uint16_t valueLength;
    //for null + 1
    char key[32+1];
    char value[128+1];
};

void debugDisplay(struct dataFormat input){
    printf("Client ID : %d\n",input.clientID);
    printf("transactionID : %d\n",input.transactionID);
    printf("put: 1, put-ack: 2, get: 3, get-ack: 4, del: 5, del-ack: 6\n");
    printf("command : %d\n",input.cmd);
    printf("none*: 0, success: 1, not exist: 2, already exist: 3\n");
    printf("code : %d\n",input.code);
    printf("Key L : %d\n",input.keyLength);
    printf("Value L : %d\n",input.valueLength);
    printf("Key : %s\n",input.key);
    printf("Value\n%s\n",input.value);
}


int main(int argc , char *argv[])
{
    int socket_desc , client_sock , c , *new_sock;
    //incoming from c0,c1,h0,h1,h2
    struct dataFormat incoming[5];
    //outgoing to c0,c1,h0,h1,h2
    struct dataFormat outgoing[5];

    //POLL RELATE VARIABLE
    struct pollfd client[5];

    int state;
    bool flagI[5];
    int roundRobin=0;


    bool isClientConnect=false;
    bool isHandlerConnect=false;

    //connect with Client
    isClientConnect=connectWithClient();
    //connect with Handler
    isHandlerConnect=connectWithHandler();

    if(isHandlerConnect&&isClientConnect){
        printf("connection process complete\n");
    }
    else{
        printf("invalid connection\n");
        printf("teminate program\n");
        exit(0);
    }


    //Poll Setting
        //ClientFD
    for(int i=0;i<2;i++){
        client[i].fd=clientFD[i];
    }
        //HandlerFD
    for(int i=0;i<3;i++){
        client[i+2].fd=handlerFD[i];
    }

        //EVEN pollin ODD pollout
    for(int i=0;i<5;i++){
        client[i].events=POLLIN;   
    }

    for(int i=0;i<5;i++){
        flagI[i]=false;
    }
    printf("start\n");
    /*-------------------------------------------------------
    Start poll
    waiting 
    -------------------------------------------------------*/
    while(1){
        //poll check - timeout 60 seconds
        state=poll(client,10,60000);
        //poll error
        if(state==-1){
            perror("poll");
            exit(0);
        }
        //timeout
        if(!state){
            printf("timeout\n");
        }

        for(int i=0;i<5;i++){
            if(client[i].revents & POLLIN){
                memset(&incoming[i],0,sizeof(struct dataFormat));
                
                read(clientFD[i],&incoming[i],sizeof(struct dataFormat));
                //debug
                //printf("-------------%d\n",i);
                //debugDisplay(incoming[i]);

                if(i<2){
                    //printf("client\n");
                    write(handlerFD[roundRobin],&incoming[i],sizeof(struct dataFormat));
                    roundRobin=(roundRobin+1)%3;                 
                }
                else{
                    //printf("handler\n");
                    //printf("will be send to client %d\n",incoming[i].clientID);
                    write(clientFD[incoming[i].clientID],&incoming[i],sizeof(struct dataFormat));                    
                }
                //printf("one done\n");
            }
        }


    }//end of infinite loop
     
     
    return 0;
}
