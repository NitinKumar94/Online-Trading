/*Header Files*/
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<string.h>
#include<stdio.h>
#include<signal.h>
#include<stdlib.h>
#include<unistd.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/shm.h>
#include<semaphore.h>
#include<sys/stat.h>
#include<sys/ipc.h>
#include<fcntl.h>
#include"specification.h"

/*global declarations*/
int sockfd;	//global socket descriptor
int role;	//documents role of user - buyer/seller

trade_set *head;

/*global function declarations*/
void interact_server(int, int, char *);		//module supplies user-commands to server
void state_role();				//user specifies role
void menu();					//menu of command oprions
int authenticate_user(int,char *);		//authenticates user
void notify_client(int);			//states success/faliure of login
void order_status();				//displays order stauts of buyer/seller
void send_request(int);				//sends buy/sell request to server
void logout(int);				//client logouts handled here
void trade_status(int);				//maintains the traded set
void insert(req_pck);				//inserts packet into clients traded set
void display();					//displays traded set

void state_role()
{
	int len;
	char comm_buffer[MAXLEN];

	printf("\nEnter your role (1-buyer / 2-seller): ");
	scanf("%d",&role);

	memset(comm_buffer,0,MAXLEN);
	sprintf(comm_buffer,"role");

	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}
	
	//send role information
	len=send(sockfd,&role,sizeof(int),0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}
}

void menu()
{
	printf("\n1. To view order status.\n");
	printf("\n2. To view trade status.\n");
	
	if(role==BUYER)
		printf("\n3. To send buy request.\n");
	else if(role==SELLER)
		printf("\n3. To send sell request.\n");

	printf("\n4. To change roles.\n");
	printf("\n5. To exit trading client.\n");
	printf("\nEnter choice: ");
}

int main(int argc, char **argv)
{
	struct sockaddr_in ServAddr;	
	
	if(argc < 3)
	{
		printf("Too less arguments to client..Terminating");
		return 0;
	}

	//creating the socket
	sockfd=socket(AF_INET,SOCK_STREAM,0); //socket(internet_family,socket_type,protocol_value) retruns socket descriptor
	if(sockfd<0)	
	{
		perror("Cannot create socket!");
		return 0;
	}

	//initializing the server socket
	ServAddr.sin_family=AF_INET;
	ServAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //using the local system IP (look back address)
	ServAddr.sin_port = htons(SERVER_PORT); //self defined server port

	if((connect(sockfd,(struct sockaddr *) &ServAddr,sizeof(ServAddr)))<0)
	{
		perror("Server is down!");
		return 0;
	}

	printf("\n connection is established :\n ");

	head=NULL;

	//MODULES FOR INTERACTING WITH SERVER
	interact_server(sockfd,atoi(argv[1]),argv[2]);
	
	return 0;
}

void interact_server(int sockfd, int trader_id, char *trader_pass)
{
	int choice,flag;	

	flag=authenticate_user(trader_id,trader_pass);
	notify_client(flag);

	state_role();

	while(1)
	{
		menu();
		scanf("%d",&choice);

		switch(choice)
		{
			case 1:
			{
				order_status();				
				break;
			}
			case 2:
			{
				//trade status module
				trade_status(trader_id);
				break;
			}
			case 3:
			{
				send_request(trader_id);				
				break;
			}
			case 4:
			{
				state_role();
				break;
			}
			case 5:
			{
				logout(trader_id);
				exit(0);
				break;
			}
			default:
			{
				printf("\nERROR IN CHOICE!!..rectify\n");				
				break;
			}
		}
	}
}

int authenticate_user(int trader_id, char *trader_pass)
{
	int len, buffer=0;
	char comm_buffer[MAXLEN];

	len=send(sockfd,&trader_id,sizeof(trader_id),0);
	if(len==0)
	{
		perror("send");
		return 0;
	}
	
	memset(comm_buffer,0,MAXLEN);
	strcpy(comm_buffer,trader_pass);
	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
		return 0;
	}

	memset(comm_buffer,0,MAXLEN);
	len=recv(sockfd,&buffer,sizeof(buffer),0);
	if(len==0)
	{
		perror("recv");
		return 0;
	}

	return(buffer);
}

void notify_client(int flag)
{
	switch(flag)
	{
		case 1:
		{
			printf("\nLogged in successfully!!\n");
			break;
		}
		case 2:
		{
			printf("\nBad password!!..terminating!!\n");
			exit(0);
		}
		case 3:
		{
			printf("\nTrader with same credentials already looged on!..terminating\n");
			exit(0);			
		}
		case 4:
		{
			printf("\nNo more logins can be accomodated!!..terminating\n");
			exit(0);			
		}
		default:
		{
			printf("\nSomething wrong in returning value %d\n",flag);
			exit(0);
		}	
	}
}

void order_status()
{
	char comm_buffer[MAXLEN];
	req_pck packet;
	int len,i;
	
	memset(comm_buffer,0,MAXLEN);
	sprintf(comm_buffer,"order status");

	printf("\n%s\n",comm_buffer);
	
	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
	}

	printf("\n\t\t\t\t********************\n");	
	printf("\nITEM NO.\t\tPRICE\t\tQUANTITY\t\tTRADER ID\n");

	for(i=0;i<MAX_ITEM;i++)
	{
		len=recv(sockfd,&packet,sizeof(req_pck),0);
		if(len==0)
		{
			perror("send");
		}

		if(packet.trader_id==0)
			printf("%d\t\t\t%s\t\t%s\t\t\t%s\n",packet.item_no,"N/A","N/A","N/A");
		else
			printf("%d\t\t\t%.2f\t\t%d\t\t\t%d\n",packet.item_no,packet.price,packet.quantity,packet.trader_id);	
	}
}

void send_request(int trader_id)
{
	char comm_buffer[MAXLEN];
	req_pck packet;
	int len;

	memset(comm_buffer,0,MAXLEN);
	sprintf(comm_buffer,"request");
	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}

	packet.trader_id=trader_id;

	printf("\nEnter item no: ");
	scanf("%d",&packet.item_no);
	printf("\nEnter quantity: ");
	scanf("%d",&packet.quantity);
	printf("\nEnter price of item: ");
	scanf("%f",&packet.price);

	printf("\n%d\t%d\t%d\t%f\n",packet.item_no,packet.trader_id,packet.quantity,packet.price);

	len=send(sockfd,&packet,sizeof(req_pck),0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}
	
	printf("\nPacket sent\n");

	memset(comm_buffer,0,MAXLEN);
	len=recv(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("recv");
		exit(0);
	}

	printf("\nWaiting here\n");

	printf("\n%s\n",comm_buffer);

	if(strcmp(comm_buffer,"Request failed!")==0)
		exit(0);
	
}

void logout(int trader_id)
{
	int len;
	char comm_buffer[MAXLEN];

	memset(comm_buffer,0,MAXLEN);
	sprintf(comm_buffer,"logout");

	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}

	
	len=send(sockfd,&trader_id,sizeof(int),0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}

	printf("\nClient terminating...\n");
}

void trade_status(int trader_id)
{
	int len,i;
	char comm_buffer[MAXLEN];
	req_pck packet;

	memset(comm_buffer,0x0,MAXLEN);
	sprintf(comm_buffer,"trade status");

	len=send(sockfd,comm_buffer,MAXLEN,0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}

	len=send(sockfd,&trader_id,sizeof(int),0);
	if(len==0)
	{
		perror("send");
		exit(0);
	}

	for(i=0;i<MAX_ITEM;i++)
	{
		//recv a packet from the server
		len=recv(sockfd,&packet,sizeof(req_pck),0);
		if(len==0)
		{
			perror("recv");
			exit(0);
		}

		if(packet.trader_id!=0)
			insert(packet);
		else
			continue;
	}

	display();
}

void insert(req_pck packet)
{
	trade_set *next,*temp;
	
	if(head!=NULL)
	{
		next=head;

		while(next->link!=NULL)
			next=next->link;
	
		temp=(trade_set *)malloc(sizeof(trade_set));
	
		temp->item_no=packet.item_no;		
		temp->trader_id=packet.trader_id;
		temp->price=packet.price;
		temp->quantity=packet.quantity;
		temp->link=NULL;

		next->link=temp;
	}
	else
	{
		temp=(trade_set *)malloc(sizeof(trade_set));

		temp->item_no=packet.item_no;
		temp->trader_id=packet.trader_id;
		temp->price=packet.price;
		temp->quantity=packet.quantity;
		temp->link=NULL;

		head=temp;
	}	
}

void display()
{
	trade_set *next;

	next=head;
	
	printf("\n\t\t\t*********************\n");
	printf("ITEM\t\tTRADER\t\tQUANTITY\t\tPRICE\n");

	while(next!=NULL)
	{
		printf("%d\t\t%d\t\t%d\t\t\t%.2f\n",next->item_no,next->trader_id,next->quantity,next->price);
		next=next->link;
	}
}
