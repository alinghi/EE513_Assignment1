#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <sys/poll.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>



static int loadBalancerFD;
static int workerFD;

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

//check out valid IP
bool isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

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

//From wiki
uint32_t jenkins_one_at_a_time_hash(const uint8_t* key, size_t length) {
	size_t i = 0;
	uint32_t hash = 0;
	while (i != length) {
		hash += key[i++];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}



//make connection with LB
//and return FD
bool connectLB(char* IP){
	struct sockaddr_in server;
	//PORT NUMBER 9000
	printf("cLB : %s\n",IP);

	//make socket
	if ( (loadBalancerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket error");
		return false;
	}

	//server address setting
	memset(&server,0,sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port   = htons(9000);

	//input IP address
	if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0){
		perror("inet_pton error for");
		return false;
	}

	//try to connect
	if (connect(loadBalancerFD, (struct sockaddr *) &server, sizeof(server)) < 0){
		perror("connect error");
		return false;
	}

	return true;
}

bool connectWorker(char* IP,int portN){
	//PORT NUMBER 8000
	struct sockaddr_in server;
	//PORT NUMBER 9000
	//printf("cLB : %s\n",IP);

	//make socket
	if ( (workerFD = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket error");
		return false;
	}

	//server address setting
	memset(&server,0,sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port   = htons(portN);
	//printf("-----port %d",portN);
	//input IP address
	if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0){
		printf("-----inet_pton error for");
		return false;
	}

	//try to connect
	if (connect(workerFD, (struct sockaddr *) &server, sizeof(server)) < 0){
		printf("-----inet_pton error for");
		return false;
	}

	return true;
}

int main(void){

	//setting IP Address
	char **workerIP;
	char temp[30];
	struct dataFormat input;
	struct dataFormat output;
	int workerPort[5];
	bool isConnect=false;
	bool workerAvailable=false;
	char* loadBalancerIP;
	workerIP=malloc(sizeof(char *) * 5);
	int counter=0;
	bool isLD=false;

	/*
	const char* workerIP[5];
	workerIP[0]="127.0.0.1";
	workerIP[1]="127.0.0.1";
	workerIP[2]="127.0.0.1";
	workerIP[3]="127.0.0.1";
	workerIP[4]="127.0.0.1";
	const char* loadBalancerIP="127.0.0.1";
	*/

	while(counter<5){
		workerIP[counter]=malloc(sizeof(char)*30);
		printf("input your IP address for %d worker: ",counter);
		scanf("%s",workerIP[counter]);
		if(isValidIpAddress(workerIP[counter]))
			counter++;
		else
			printf("invalid IP addr \n");
	}
	while(!isLD){
	printf("input your IP address for LoadBalancer: ");
	loadBalancerIP=malloc(sizeof(char)*30);
	scanf("%s",loadBalancerIP);
	isLD=isValidIpAddress(loadBalancerIP);
	}
	

	

	workerPort[0]=8000;
	workerPort[1]=8001;
	workerPort[2]=8002;
	workerPort[3]=8003;
	workerPort[4]=8004;
	uint32_t hashResult=0;
	int designateWorker=0;
	int state;
	//request from LB or to LB
		//0 LB try to write
		//1 Handler try to write
	struct pollfd request[2];

	isConnect=connectLB(loadBalancerIP);
	if(isConnect){
		printf("connected\n");
	}
	else{
		printf("connection error with LB\n");
		printf("terminate\n");
		exit(1);
	}
	request[0].fd=loadBalancerFD;
	request[1].fd=loadBalancerFD;
	request[0].events=POLLIN;
	request[1].events=POLLOUT;

	//make sure other 2 handler's are ready

	while(1){
		//
		state=poll(request,2,10000);
		if(state==-1){
			perror("poll");
			exit(0);
		}

		if(!state){
			printf("timeout\n");
		}

		//start if
		//read from LB
		if(request[0].revents & POLLIN){
            		memset(&input,0,sizeof(struct dataFormat));
            		read(loadBalancerFD,&input,sizeof(struct dataFormat));
        			//printf("Check round robin\n");


        			hashResult=jenkins_one_at_a_time_hash(input.key,input.keyLength);
        			designateWorker=hashResult%5;
        			//printf("designateWorker %d\n",designateWorker);
        			//printf("Hash %u\n",hashResult);
        			while(!workerAvailable){
        			workerAvailable=connectWorker(workerIP[designateWorker],workerPort[designateWorker]);        				
        			usleep(100);
        			}
        			//printf("Socket n %d",workerFD);
        			//printf("----input----\n");
        			//debugDisplay(input);
        			//printf("write to worker\n");
            		write(workerFD,&input,sizeof(struct dataFormat));
            		//read result from worker
            		memset(&output,0,sizeof(struct dataFormat));

            		read(workerFD,&output,sizeof(struct dataFormat));
            		
            		//printf("read from worker\n");
        			//printf("----output----\n");
            		//debugDisplay(output);
            		write(loadBalancerFD,&output,sizeof(struct dataFormat));
            		workerAvailable=false;
        /*    	
		//calculate hash
            	
		//write to one of workers
            	
            		//connect and get FD
            			//todo
            		//write to worker 

		*/
		//write to LB
            	
        }//finish of if
	}

	return 0;
}