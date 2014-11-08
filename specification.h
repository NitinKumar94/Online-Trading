/*Header file containing definition of data structures used.
Symbolic definitions, along with specifications for sockets,
shared memory keys and named POSIX semaphores. Also contains 
maximum length of buffers used to exchange messages using sockets.*/

/*Symbolic definitions*/
#define BUYER 1
#define SELLER 2
#define BUYKEY 5496
#define SELLKEY 6050
#define LOGINKEY 7500
#define SEMBUY "/sembuy"
#define SEMSELL "/semsell"
#define SEMLOGIN "/semlogin"
#define MAXLEN 256
#define SERVER_PORT 27032
#define MAX_ITEM 5
#define MAX_TRADER 5
#define SUCESLOG 1
#define BADPASS 2
#define LOGDON 3
#define NOSPACE 4
#define TERMINATE -256

/*Buy Sell data sturctures specifications*/
typedef struct _item_details
{
	int trader_id;
	int quantity;
	double price;
}item_details;

typedef struct _item_list
{
	int item_no;
	item_details *details;
}item_list;

/*login authentication data structure*/
typedef struct _login
{
	int trader_id;
	char password[MAXLEN];
	int login_status;
}login; 

/*Request Packet*/
typedef struct _req_pck
{
	int item_no;
	int trader_id;
	int quantity;
	float price;
}req_pck;

/*traded set structure specification*/
typedef struct _trade_set
{
	int item_no;
	int trader_id;
	int quantity;
	float price;
	struct _trade_set *link;
}trade_set;
