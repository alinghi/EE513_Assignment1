#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <sys/poll.h>
#include <stdint.h>
#include <stdbool.h>

static int listenFD;
static bool failCheck;
struct Node *root;
FILE * fp;

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

// An AVL tree node
struct Node
{
    uint32_t key;
    struct dataFormat data;
    struct Node *left;
    struct Node *right;
    int height;
};
 
// A utility function to get maximum of two integers
int max(int a, int b);
 
// A utility function to get height of the tree
int height(struct Node *N)
{
    if (N == NULL)
        return 0;
    return N->height;
}
 
// A utility function to get maximum of two integers
int max(int a, int b)
{
    return (a > b)? a : b;
}
 
/* Helper function that allocates a new node with the given key and
    NULL left and right pointers. */
struct Node* newNode(uint32_t key,struct dataFormat input)
{
    struct Node* node = (struct Node*) malloc(sizeof(struct Node));
    node->key   = key;
    node->data=input;
    node->left   = NULL;
    node->right  = NULL;
    node->height = 1;  // new node is initially added at leaf
    return(node);
}
 
// A utility function to right rotate subtree rooted with y
// See the diagram given above.
struct Node *rightRotate(struct Node *y)
{
    struct Node *x = y->left;
    struct Node *T2 = x->right;
 
    // Perform rotation
    x->right = y;
    y->left = T2;
 
    // Update heights
    y->height = max(height(y->left), height(y->right))+1;
    x->height = max(height(x->left), height(x->right))+1;
 
    // Return new root
    return x;
}
 
// A utility function to left rotate subtree rooted with x
// See the diagram given above.
struct Node *leftRotate(struct Node *x)
{
    struct Node *y = x->right;
    struct Node *T2 = y->left;
 
    // Perform rotation
    y->left = x;
    x->right = T2;
 
    //  Update heights
    x->height = max(height(x->left), height(x->right))+1;
    y->height = max(height(y->left), height(y->right))+1;
 
    // Return new root
    return y;
}
 
// Get Balance factor of node N
int getBalance(struct Node *N)
{
    if (N == NULL)
        return 0;
    return height(N->left) - height(N->right);
}

struct Node* find(struct Node * node, uint32_t key){

	if(node==NULL)
	{
		failCheck=true;
		return NULL;
	}
    if (key < node->key)
        find(node->left, key);
    else if (key > node->key)
        find(node->right, key);
    else // Equal keys
    {
        return node;
    }

}

struct Node* insert(struct Node* node, uint32_t key,struct dataFormat input)
{
	//debug purpose
	//printf("insert\n");
    /* 1.  Perform the normal BST rotation */
    if (node == NULL){
    	//printf("newnode\n");
        return(newNode(key,input));
    }
 
    if (key < node->key)
        node->left  = insert(node->left, key, input);
    else if (key > node->key)
        node->right = insert(node->right, key, input);
    else // Equal keys not allowed
    {
    	failCheck=true;
        return node;
    }
 
    /* 2. Update height of this ancestor node */
    node->height = 1 + max(height(node->left),height(node->right));
 
    /* 3. Get the balance factor of this ancestor
          node to check whether this node became
          unbalanced */
    int balance = getBalance(node);
 
    // If this node becomes unbalanced, then there are 4 cases
 
    // Left Left Case
    if (balance > 1 && key < node->left->key)
        return rightRotate(node);
 
    // Right Right Case
    if (balance < -1 && key > node->right->key)
        return leftRotate(node);
 
    // Left Right Case
    if (balance > 1 && key > node->left->key)
    {
        node->left =  leftRotate(node->left);
        return rightRotate(node);
    }
 
    // Right Left Case
    if (balance < -1 && key < node->right->key)
    {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }
 
    /* return the (unchanged) node pointer */
    return node;
}
 
/* Given a non-empty binary search tree, return the
   node with minimum key value found in that tree.
   Note that the entire tree does not need to be
   searched. */
struct Node * minValueNode(struct Node* node)
{
    struct Node* current = node;
 
    /* loop down to find the leftmost leaf */
    while (current->left != NULL)
        current = current->left;
 
    return current;
}
 
// Recursive function to delete a node with given key
// from subtree with given root. It returns root of
// the modified subtree.
struct Node* deleteNode(struct Node* root, uint32_t key)
{
    // STEP 1: PERFORM STANDARD BST DELETE
 	//printf("delete\n");
    if (root == NULL)
    {
        failCheck=true;
        return root;
    }
 
    // If the key to be deleted is smaller than the
    // root's key, then it lies in left subtree
    if ( key < root->key ){
        //printf("step2->left\n");
        root->left = deleteNode(root->left, key);
    }
 
    // If the key to be deleted is greater than the
    // root's key, then it lies in right subtree
    else if( key > root->key ){
        //printf("step2->right\n");
        root->right = deleteNode(root->right, key);
    }
 
    // if key is same as root's key, then This is
    // the node to be deleted
    else
    {
        //printf("step2->find\n");
        // node with only one child or no child
        if( (root->left == NULL) || (root->right == NULL) )
        {
            struct Node *temp = root->left ? root->left : root->right;
 
            // No child case
            if (temp == NULL)
            {
                temp = root;
                root = NULL;
            }
            else // One child case
             *root = *temp; // Copy the contents of
                            // the non-empty child
            free(temp);
        }
        else
        {
            // node with two children: Get the inorder
            // successor (smallest in the right subtree)
            struct Node* temp = minValueNode(root->right);
 
            // Copy the inorder successor's data to this node
            root->key = temp->key;
 
            // Delete the inorder successor
            root->right = deleteNode(root->right, temp->key);
        }
    }
 
    // If the tree had only one node then return
    if (root == NULL){
        //printf("second null case\n");
    	return root;
    }
 
    // STEP 2: UPDATE HEIGHT OF THE CURRENT NODE
    root->height = 1 + max(height(root->left),height(root->right));
 
    // STEP 3: GET THE BALANCE FACTOR OF THIS NODE (to
    // check whether this node became unbalanced)
    int balance = getBalance(root);
 
    // If this node becomes unbalanced, then there are 4 cases
 
    // Left Left Case
    if (balance > 1 && getBalance(root->left) >= 0)
        return rightRotate(root);
 
    // Left Right Case
    if (balance > 1 && getBalance(root->left) < 0)
    {
        root->left =  leftRotate(root->left);
        return rightRotate(root);
    }
 
    // Right Right Case
    if (balance < -1 && getBalance(root->right) <= 0)
        return leftRotate(root);
 
    // Right Left Case
    if (balance < -1 && getBalance(root->right) > 0)
    {
        root->right = rightRotate(root->right);
        return leftRotate(root);
    }
 
    return root;
}
 
// A utility function to print preorder traversal of
// the tree.
// The function also prints height of every node
void preOrder(struct Node *root)
{
    if(root != NULL)
    {
        printf("%u ", root->key);
        printf("%s\n",root->data.value);
        preOrder(root->left);
        preOrder(root->right);
    }
}


void processQuery(){
	int connfd,c;
	failCheck=false;
	int n;
	uint32_t hashResult;
	struct sockaddr_in client_addr;
	struct Node* temp;
	c = sizeof(struct sockaddr_in); 
	struct dataFormat input,output;

	//Debug purpose printf
	//printf("query part1\n");
	connfd=accept(listenFD, (struct sockaddr *)&client_addr, (socklen_t*)&c);
	if(connfd==-1){
		printf("accept error\n");
		printf("teminate the program\n");
		exit(0);
	}
	if(n=recv(connfd,&input,sizeof(struct dataFormat),0)<0)
	{
		printf("read error\n");
		printf("teminate the program\n");
		exit(0);		
	}
	

	//Debug purpose printf
	//preOrder(root);

    /*-------------------------------------------------------
	ACTUAL PROCESS
    -------------------------------------------------------*/
    memcpy(&output,&input,sizeof(struct dataFormat));
    hashResult=jenkins_one_at_a_time_hash(input.key,input.keyLength);
    output.cmd=input.cmd+1;
    //put
    if(input.cmd==1){
    	root=insert(root,hashResult,input);
    	//preOrder(root);
    	if(failCheck){
    		//printf("fail check\n");
    		output.code=3;
    	}
    	else{
    		output.code=1;
    	}
    }
    //get
    else if(input.cmd==3){
    	temp=find(root,hashResult);
    	//if not found
    	if(failCheck){
    		//printf("fail check\n");
    		output.code=2;
    	}
    	//found
    	else{
    		//printf("=========================\n");
    		//debugDisplay(temp->data);
    		//printf("=========================\n");
    		strcpy(output.value,(temp->data).value);
    		output.code=1;	
    	}    	
    }
    //del
    else{
    	root=deleteNode(root,hashResult);
    	if(failCheck){
    		//printf("fail check\n");
    		output.code=2;
    	}
    	else{
    		output.code=1;
    	}
    }
    //printf("--------output packet----");
    //debugDisplay(output);
    write(connfd,&output,sizeof(struct dataFormat));
	//Debug purpose printf
	//printf("query part3\n");

	close(connfd);
}


int main( int argc, char* argv[] ){
    int c;
    struct sockaddr_in server;
    int accessPort;
    //setup AVL TREE
    root=NULL;
    if(argc!=2){
    	printf("<Usage> ./worker #ofWorker[0-4]\n");
    	exit(0);
    }

    if(strlen(argv[1])!=1){
    	printf("Did not put single digit in worker\n");
    	exit(0);    	
    }

    accessPort=argv[1][0]-48;
    if(accessPort>4){
    	printf("Incorrect worker number");
    	exit(0);    	
    }

    int workerPort[5];
	workerPort[0]=8000;
	workerPort[1]=8001;
	workerPort[2]=8002;
	workerPort[3]=8003;
	workerPort[4]=8004;
    //Create socket
    listenFD = socket(AF_INET , SOCK_STREAM , 0);
    if (listenFD == -1)
    {
        printf("Could not create socket");
        return false;
    }
    puts("Socket created");
     
    //Prepare the sockaddr_in structure
    memset(&server,0,sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    printf("port number %d\n",accessPort);
    printf("port number %d\n",workerPort[accessPort]);
    server.sin_port = htons(workerPort[accessPort]);
     
    //Bind
    if( bind(listenFD,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return false;
    }
    puts("bind done");
    c = sizeof(struct sockaddr_in); 
    //Listen
    listen(listenFD , 2);




	while(1){
		processQuery();
	}	
	return 0;
}