//Preprocessor
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

//all boolean var should be false at init
//if not your code is wrong(maybe forgot after debugging)
static bool isConnect=false;
static bool isSet=false;
static bool finishSetting=false;
static int clientNumber=0;
static int socketFD;
static int gTransactionID=0;
FILE * fp;

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

void printResult(struct dataFormat input){
	if(input.cmd%2==1){
		printf("This is not ack packet\n");
	}
	else{
		//put
		if(input.cmd==2){
			if(input.code==1){
				printf("Success\n");
			}
			else if(input.code==3){
				printf("Fail(reason: already exist)\n");
			}
			else{
				printf("Fail - return code is invalid\n");
			}
		}
		//get
		else if(input.cmd==4){
			//print out value if success
			if(input.code==1){
				printf("[%s]\n",input.value);
			}
			//fail - case not exist
			else if(input.code==2){
				printf("Fail(reason: not exist)\n");
			}
			else{
				printf("Fail - return code is invalid\n");
			}			
		}
		//del
		else if(input.cmd==6){
			if(input.code==1){
				printf("Success\n");
			}
			else if(input.code==2){
				printf("Fail(reason: not exist)\n");
			}
			else{
				printf("Fail - return code is invalid\n");
			}				
		}
		else{
			printf("This packet is invalid\n");
		}
	}
}

//Should not ctrl+D : EOF never
//Client number collision will not be handle.
//If client number is same : some of the data will be missing.



//actual client after setting
void cli(){

	//varible
	const char* keyword_1="put";
	const char* keyword_2="get";
	const char* keyword_3="del";

	//if packet is ready
	bool isSend=false;
	char buf[201];

	//check out input argument
	int argc=0;
	char * argv[4];
	struct dataFormat outgoing;
	struct dataFormat incoming;

	//start while
	while(1){
	//start program
	printf("Client #%d>",clientNumber);
	argc=0;
	memset(&outgoing,0,sizeof(struct dataFormat));
	memset(&incoming,0,sizeof(struct dataFormat));
	outgoing.clientID=clientNumber;
	outgoing.code=0;
	memset(buf,0,sizeof(buf));
	fgets(buf,200,stdin);

	//convert last \n to null
	buf[strlen(buf)-1]=0;

	//separate by space
	argv[0] = strtok (buf," ");
	argv[1] = strtok(NULL," ");
	argv[2] = strtok(NULL," ");
	argv[3] = strtok(NULL," ");

	//calculate out number of argument
	for(int i=0;i<4;i++){
		if(argv[i])
			argc++;
	}

	//too many argument
	if(argc>3)
	{
		printf("Too many argument in input\n");
	}

	//check out command

	//Case1 : put
	if(strcmp(keyword_1,argv[0])==0){
		/*-------------------------------------------------------
		Check validation of input
		-------------------------------------------------------*/
		// # of argument is wrong
		if(argc!=3){
			printf("<Usage> put key value\n");
			continue;
		}

		//Check key's length is smaller than 32
		if(strlen(argv[1])>32){
			printf("Key length should be smaller than 32\n");
			continue;
		}
		//Check value's length is smaller than 128
		if(strlen(argv[2])>128){
			printf("Value length should be smaller than 128\n");
			continue;
		}


		/*-------------------------------------------------------
		Fill the field of outgoing
		-------------------------------------------------------*/
		//uint32_t clientID; -> set at init
		//uint32_t transactionID;
			//Input Transaction ID
			outgoing.transactionID=gTransactionID;
			//Update Transaction ID
			gTransactionID++;
		//uint16_t cmd;
			//Input cmd
			outgoing.cmd=1;
		//uint16_t code; -> set at init
		//uint16_t keyLength;
			//Input key Length
			outgoing.keyLength=strlen(argv[1]);
		//uint16_t valueLength;
			outgoing.valueLength=strlen(argv[2]);
		//char key[32+1];
			strcpy(outgoing.key,argv[1]);
		//char value[128+1];
			strcpy(outgoing.value,argv[2]);
	}

	//Case2 : get
	else if(strcmp(keyword_2,argv[0])==0){
		/*-------------------------------------------------------
		Check validation of input
		-------------------------------------------------------*/
		// # of argument is wrong
		if(argc!=2){
			printf("<Usage> get key\n");
			continue;
		}

		//Check key's length is smaller than 32
		if(strlen(argv[1])>32){
			printf("Key length should be smaller than 32\n");
			continue;
		}

		/*-------------------------------------------------------
		Fill the field of outgoing
		-------------------------------------------------------*/
		//uint32_t clientID; -> set at init
		//uint32_t transactionID;
			//Input Transaction ID
			outgoing.transactionID=gTransactionID;
			//Update Transaction ID
			gTransactionID++;
		//uint16_t cmd;
			//Input cmd
			outgoing.cmd=3;
		//uint16_t code; -> set at init
		//uint16_t keyLength;
			//Input key Length
			outgoing.keyLength=strlen(argv[1]);
		//uint16_t valueLength; -> don't need to set
		//char key[32+1];
			strcpy(outgoing.key,argv[1]);
		//char value[128+1]; -> don't need to set





	}

	//Case3 : del
	else if(strcmp(keyword_3,argv[0])==0){
		/*-------------------------------------------------------
		Check validation of input
		-------------------------------------------------------*/
		// # of argument is wrong
		if(argc!=2){
			printf("<Usage> del key\n");
			continue;
		}

		//Check key's length is smaller than 32
		if(strlen(argv[1])>32){
			printf("Key length should be smaller than 32\n");
			continue;
		}

		/*-------------------------------------------------------
		Fill the field of outgoing
		-------------------------------------------------------*/
		//uint32_t clientID; -> set at init
		//uint32_t transactionID;
			//Input Transaction ID
			outgoing.transactionID=gTransactionID;
			//Update Transaction ID
			gTransactionID++;
		//uint16_t cmd;
			//Input cmd
			outgoing.cmd=5;
		//uint16_t code; -> set at init
		//uint16_t keyLength;
			//Input key Length
			outgoing.keyLength=strlen(argv[1]);
		//uint16_t valueLength; -> don't need to set
		//char key[32+1];
			strcpy(outgoing.key,argv[1]);
		//char value[128+1]; -> don't need to set

	}

	//Other case : input fail
	else{
		printf("Invalid command\n");
		continue;
	}

	//send//debug
	//debugDisplay(outgoing);
	write(socketFD,&outgoing,sizeof(struct dataFormat));
	//printResult(outgoing);
	//wait result
	memset(&incoming,0,sizeof(struct dataFormat));
	//printf("read\n----");
	read(socketFD,&incoming,sizeof(struct dataFormat));
	//print out result
	//debugDisplay(incoming);
	printResult(incoming);

	//finish while
	}

}

//check out IP address
bool isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

//set connection and client number
bool setClient(){

	//Only two command accepted in this client
		//Command 1 : connect <ipAddr>
		//Command 2 : set 0 or set 1
	char buf[81];
	bool validIP=false;
	const char* keyword_1="connect";
	const char* keyword_2="set";
	const char* number_0="0";
	const char* number_1="1";

	struct sockaddr_in server;

	int argc=0;
	char * argv[3];
	printf("Client> ");
	memset(buf,0,sizeof(buf));
	fgets(buf,80,stdin);

	//convert last \n to null
	buf[strlen(buf)-1]=0;

	//separate by space
	argv[0] = strtok (buf," ");
	argv[1] = strtok(NULL," ");
	argv[2] = strtok(NULL," ");

	//calculate number of argument
	for(int i=0;i<3;i++){
		if(argv[i])
			argc++;
	}

	//if more or less argument inputted restart
	if(argc!=2){
		printf("too many or too less argument\n");
		return false;
	}

	//in case of : "connect" command
	if(strcmp(argv[0],keyword_1)==0){
		//if connection exist restart this function
		if(isConnect){
			printf("already connected\n");
			return false;
		}
		printf("try to connect\n");

		//check out validation of IP
		validIP=isValidIpAddress(argv[1]);

		//if valid IP addr try to connect
		if(validIP){
			//make socket
			if ( (socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0){
				perror("socket error");
				return false;
			}

			//server address setting
			memset(&server,0,sizeof(server));
			server.sin_family = AF_INET;
			server.sin_port   = htons(5131);

			//input IP address
			if (inet_pton(AF_INET, argv[1], &server.sin_addr) <= 0){
				perror("inet_pton error for");
				return false;
			}

			//try to connect
			if (connect(socketFD, (struct sockaddr *) &server, sizeof(server)) < 0){
				perror("connect error");
				return false;
			}

			//every socket thing success?
			printf("connection success\n");
			isConnect=true;
		}
		//if input IP Addr is invalid => restart this function
		else{
			return false;
		}
	}

	//in case of : "set" command
		//number of client can varied during this phase
		//but after setClient() function call finished => number of client is fixed.
	else if(strcmp(argv[0],keyword_2)==0){
		if(strcmp(argv[1],number_0)==0){
			printf("set to 0\n");
			clientNumber=0;
			isSet=true;
		}
		else if(strcmp(argv[1],number_1)==0){
			printf("set to 1\n");
			clientNumber=1;
			isSet=true;
		}
		else{
			printf("client number should be 0 or 1\n");
		}
	}

	//in case of : wrong operation
	else{
		printf("invalid operation\n");
		//restart
		return false;
	}

	//check out more setting is needed or not and return it.
	return (isSet && isConnect);

}



int main( int argc, char* argv[] ){
	while(!finishSetting){
		//setting client
		finishSetting=setClient();
	}
	printf("Setting finished\n");
	printf("Client number you choose : %d\n",clientNumber);

	cli();
	
	return 0;
}