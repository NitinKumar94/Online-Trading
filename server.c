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

int listensd, connsd;		//socket descriptors
int role;

/*global semaphore and shared memory specifications*/
sem_t *sem_login, *sem_buy, *sem_sell;
int sell_id,buy_id,login_id;
key_t sell_key,buy_key,login_key;

/*global function declarations*/
int authentication(int,login *);
void order_status(int,item_list *);
void request(int,item_list *);
void logout(int,login *);
void print_structure(item_list *);
void initialize_login(int);
void initialize_item_list(int);
void signal_handler(int);	//closes and unlinks semaphores on ctrl C signal 
void print_login(login *);
void trade_status(int,item_list *,item_list *);

int main(int argc, char **argv)
{
	int Clilen; //Clilen is the length of the client socket, used as a value-result argument
	pid_t childpid; //holds process id of child
	struct sockaddr_in ServAddr, CliAddr; //sockaddr structure for sockets; one for server and the other for client
	sem_t *semlogin,*semsell,*sembuy;
	int buyid,sellid,loginid;

	/*Creating shared memory segments*/

	/*Login Array*/
	if((login_id=shmget(login_key,MAX_TRADER * sizeof(login),IPC_CREAT | 0666))<0)	//if return value of shmget < 0 : Error! 
	{
		perror("shmget");
		exit(1);
	}

	/*Buy Queue*/
	if((buy_id=shmget(buy_key,MAX_ITEM * sizeof(item_list),IPC_CREAT | 0666))<0)	//if return value of shmget < 0 : Error! 
	{
		perror("shmget");
		exit(1);
	}

	/*Sell Queue*/
	if((sell_id=shmget(sell_key,MAX_ITEM * sizeof(item_list),IPC_CREAT | 0666))<0)	//if return value of shmget < 0 : Error! 
	{
		perror("shmget");
		exit(1);
	}

	/*initialization methods*/
	//initialize login array
	initialize_login(login_id);

	//initialize item lists
	initialize_item_list(buy_id);
	initialize_item_list(sell_id);


	//creating the socket
	listensd=socket(AF_INET,SOCK_STREAM,0); //socket(internet_family,socket_type,protocol_value) retruns socket descriptor
	if(listensd<0)	
	{
		perror("Cannot create socket!");
		return 0;
	}

	//initializing the server socket
	ServAddr.sin_family=AF_INET;
	ServAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //using the local system IP (loop back address)
	ServAddr.sin_port = htons(SERVER_PORT); //self defined server port

	//binding socket
	if(bind(listensd,(struct sockaddr *) &ServAddr, sizeof(ServAddr))<0)
	{
		perror("Cannot bind port!");
		return 0;
	}

	//defining number of clients that can connect through SERVER_PORT , listen() indicates that server is ready for connections
	listen(listensd,MAX_TRADER);

	/*Initiating signal handler*/
	signal(SIGINT,signal_handler);

	/*Creating semaphores*/	
	sem_login=sem_open(SEMLOGIN,O_CREAT,0666,1); 	//semaphore for login queue
	sem_buy=sem_open(SEMBUY,O_CREAT,0666,1); 	//semaphore for buy queue
	sem_sell=sem_open(SEMSELL,O_CREAT,0666,1);	//semaphore for sell queue

	/*Setting predefined key values for shared memory segments*/
	login_key=LOGINKEY;
	buy_key=BUYKEY;
	sell_key=SELLKEY;

	

	/*Server runs infinite loop*/
	for(;;)
	{
		Clilen=sizeof(CliAddr);
		if((connsd=accept(listensd,(struct sockaddr *)&CliAddr,&Clilen))<0)
		{
			perror("Cannot establish connection!");
			return 0;
		}

		if((childpid=fork())==0)
		{
			close(listensd); //child process closes its copy of the listening socket since it is going to service clients through connsd
			
			printf("\nRequest Serviced with child process %d\n",getpid());
			
			//call a module for servicing clients
			service_client(connsd,getpid());

			close(connsd); //child closes its version of connsd after computation is done (return from childprocess())
			exit(1); //child terminates
		}

		close(connsd); //parent closes the connected socket and begins listening for more connections
	}

	return 0;
}

int service_client(int connsd, int id)
{
	item_list *buyptr,*sellptr;
	login *loginptr;
	int flag,len;
	char comm_choice[MAXLEN];

	/*attaching shared memory segments*/
	//login array	
	if((loginptr=(login *)shmat(login_id,0,0))==(login *)-1)
	{
		perror("shmat");
		return 0;
	}

	//buy queue
	if((buyptr=(item_list *)shmat(buy_id,0,0))==(item_list *)-1)
	{
		perror("shmat");
		return 0;
	}

	//sell queue
	if((sellptr=(item_list *)shmat(sell_id,NULL,0))==(item_list *)-1)
	{
		perror("shmat");
		return 0;
	}

	//method for authenticating login.
	sem_wait(sem_login);
	flag=authentication(connsd,loginptr);
	sem_post(sem_login);

	if(flag==0)
	{
		//send login failed message to client
		//terminate child process
		exit(0);
	}

	/*sem_wait(sem_login);
	print_login(loginptr);
	sem_post(sem_login);*/

	while(1)
	{
		memset(comm_choice,0,MAXLEN);
		len=recv(connsd,comm_choice,MAXLEN,0);
		if(len==0)
		{
			perror("recv");
			exit(0);
		}
	
		if(strcmp(comm_choice,"order status")==0)
		{
			if(role==BUYER)
			{	
				sem_wait(sem_sell);		
				order_status(connsd,sellptr);
				sem_post(sem_sell);
			}
			else if(role==SELLER)
			{
				sem_wait(sem_buy);		
				order_status(connsd,buyptr);
				sem_post(sem_buy);
			}
		}
		else if(strcmp(comm_choice,"trade status")==0)
		{
			sem_wait(sem_buy);
			sem_wait(sem_sell);

			if(role==BUYER)
			{
				trade_status(connsd,buyptr,sellptr);
			}
			else if(role==SELLER)
			{
				trade_status(connsd,sellptr,buyptr);
			}

			sem_post(sem_sell);
			sem_post(sem_buy);	
		}
		else if(strcmp(comm_choice,"request")==0)
		{		

			if(role==BUYER)
			{
				printf("\nBEFORE\n");				

				sem_wait(sem_buy);
				print_structure(buyptr);
				sem_post(sem_buy);				

				sem_wait(sem_buy);
				request(connsd,buyptr);
				sem_post(sem_buy);

				printf("\nAFTER\n");

				sem_wait(sem_buy);
				print_structure(buyptr);
				sem_post(sem_buy);

			}			
			else if(role==SELLER)
			{
				sem_wait(sem_sell);				
				request(connsd,sellptr);
				sem_post(sem_sell);
			}
		}
		else if(strcmp(comm_choice,"role")==0)
		{
			
			len=recv(connsd,&role,sizeof(int),0);
			if(len==0)
			{
				perror("recv");
				exit(0);
			}
		}
		else if(strcmp(comm_choice,"logout")==0)
		{
			logout(id,loginptr);
			exit(0);
		}
		else
		{
			printf("\nError sending command..\n");
			exit(0);
		}
	}

}

int authentication(int connsd, login *loginptr)
{
	int len,id=0,flag=0,buffer,i;
	char pass[MAXLEN];

	len=recv(connsd,&id,sizeof(int),0);
	if(len==0)
	{
		perror("recv");
		return 0;
	}

	len=recv(connsd,pass,MAXLEN,0);
	if(len==0)
	{
		perror("recv");
		return 0;
	}

	//code for checking
	for(i=0;i<MAX_TRADER;i++)
	{
				
		if(loginptr[i].login_status==-1)	//condition for first time login
		{
			loginptr[i].login_status=1;
			loginptr[i].trader_id=id;
			strcpy(loginptr[i].password,pass);
			flag=1;

			//successful login message
			buffer=SUCESLOG;
			len=send(connsd,&buffer,sizeof(int),0);
			if(len==0)
			{
				perror("send");
				return 0;
			}
			
			break;
		}
		else if(loginptr[i].trader_id==id)	//condition for re-login
		{
			if(loginptr[i].login_status==0)
			{			
				if(strcmp(loginptr[i].password,pass)==0)
				{
					loginptr[i].login_status=1;
					//send successful login message
					buffer=SUCESLOG;
					len=send(connsd,&buffer,sizeof(int),0);
					if(len==0)
					{
						perror("send");
						return 0;
					}
				}
				else
				{
					//send bad password message
					buffer=BADPASS;
					len=send(connsd,&buffer,sizeof(int),0);
					if(len==0)
					{
						perror("send");
						return 0;
					}

				}
			}
			else
			{
				//send already logged on message
				buffer=LOGDON;
				len=send(connsd,&buffer,sizeof(int),0);
				if(len==0)
				{
					perror("send");
					return 0;
				}
			}
	
			flag=1;
			break;
		}
		else	//look in next location
			continue;
	}

	if(flag==0)
	{
		//send cannont accomodate more clients
		buffer=NOSPACE;
		len=send(connsd,&buffer,sizeof(int),0);
		if(len==0)
		{
			perror("send");
			return 0;
		}
		//terminate connection
		exit(0);
	}
}

void order_status(int connsd, item_list *ptr)
{
	req_pck packet;
	int i,j,len;

	for(i=0;i<MAX_ITEM;i++)
	{
		
		packet.item_no=0;
		packet.trader_id=0;
		packet.quantity=0;

		if(role==BUYER)
			packet.price=10000;
		if(role==SELLER)
			packet.price=0;

		packet.item_no=ptr[i].item_no;

		for(j=0;j<MAX_TRADER;j++)
		{
			
			if(role==SELLER)
			{
				if(packet.price < ptr[i].details[j].price && ptr[i].details[j].quantity!=0)
				{
					packet.trader_id=ptr[i].details[j].trader_id;
					packet.quantity=ptr[i].details[j].quantity;
					packet.price=ptr[i].details[j].price;
				}
			}
			else if(role==BUYER)
			{
				if(packet.price > ptr[i].details[j].price && ptr[i].details[j].quantity!=0)
				{
					packet.trader_id=ptr[i].details[j].trader_id;
					packet.quantity=ptr[i].details[j].quantity;
					packet.price=ptr[i].details[j].price;
				}
			}
		}

		len=send(connsd,&packet,sizeof(req_pck),0);
		if(len==0)
		{
			perror("send");
			exit(0);
		}
	}
}
void request(int connsd, item_list *ptr)
{
	int len,flag=0,index,i;
	req_pck packet;
	char comm_buffer[MAXLEN];	

	len=recv(connsd,&packet,sizeof(req_pck),0);
	if(len==0)
	{
		perror("recv");
		exit(0);
	}

	//process to insert the packet in the queue
	
	for(i=0;i<MAX_ITEM;i++)
	{
		if(packet.item_no==ptr[i].item_no)
		{
			index=i; //printf("\nitem found %d\n", ptr[index].item_no);
			break;	
		}
	}

	for(i=0;i<MAX_TRADER;i++)
	{		

		if(ptr[index].details[i].trader_id == 0)	//condition checking for details being entered by a trader for the first time
		{		
			ptr[index].details[i].trader_id=packet.trader_id;
			ptr[index].details[i].quantity=packet.quantity;
			ptr[index].details[i].price=packet.price;

			//printf("\nDetails being entered first\n");
			
			flag=1;
			break;
		}
		else if(ptr[index].details[i].trader_id == packet.trader_id)	//condition for updating details by a particular trader
		{
			ptr[index].details[i].trader_id=packet.trader_id;
			ptr[index].details[i].quantity=packet.quantity;	
			ptr[index].details[i].price=packet.price;

			//printf("\nUpdate of details!!\n");
		
			flag=1;
			break;
		}
	}


	memset(comm_buffer,0,MAXLEN);
	
	if(flag==0)
	{
		sprintf(comm_buffer,"Request failed!");
		len=send(connsd,comm_buffer,MAXLEN,0);
		if(len==0)
		{
			perror("send");
			exit(0);
		}
	}
	else if(flag==1)
	{
		sprintf(comm_buffer,"Request successful!");
		len=send(connsd,comm_buffer,MAXLEN,0);
		if(len==0)
		{
			perror("send");
			exit(0);
		}
	}
	else
	{
		printf("\nError in flag return rquest module..\n");
		exit(0);
	}
}

void logout(int pid, login *loginptr)
{
	int len,trader_id,i;
	
	len=recv(connsd,&trader_id,sizeof(int),0);
	if(len==0)
	{
		perror("recv");
		exit(0);
	}

	for(i=0;i<MAX_TRADER;i++)
	{
		sem_wait(sem_login);
		
		if(loginptr[i].trader_id==trader_id)
		{
			loginptr[i].login_status=0;
		}

		sem_post(sem_login);
	}

	printf("\nClient pid=%d terminated..\n",pid);
}

void print_structure(item_list *ptr)
{
	int i,j;

	for(i=0;i<MAX_ITEM;i++)
	{
		printf("\n\n%d\n",ptr[i].item_no);		
		for(j=0;j<MAX_TRADER;j++)
		{
			printf("%d\t%d\t%f\n",ptr[i].details[j].trader_id,ptr[i].details[j].quantity,ptr[i].details[j].price);
		}
	}
}

void signal_handler(int sig)
{
	printf("\nSignal caught\n");	

	sem_close(sem_buy);
	sem_close(sem_sell);
	sem_close(sem_login);
	sem_unlink(SEMBUY);
	sem_unlink(SEMSELL);
	sem_unlink(SEMLOGIN);

	exit(1);
}

void initialize_login(int loginid)
{
	int i;
	login *logptr;

	if((logptr=(login *)shmat(loginid,NULL,0))==(login *)-1)
	{
		perror("shmat");
	}

	for(i=0;i<MAX_TRADER;i++)
	{
		logptr[i].trader_id=-1;
		logptr[i].login_status=-1;
		memset(logptr[i].password,0,MAXLEN);
	} 

	shmdt(logptr);
}

void initialize_item_list(int queue_id)
{
	int i,j,ctr=101;
	item_list *ptr;
	int details_id;

	if((ptr=(item_list *)shmat(queue_id,NULL,0))==(item_list *)-1)
	{
		perror("shmat");
	}

	//create array of locations for each item
	for(i=0;i<MAX_ITEM;i++)
	{
		ptr[i].item_no=ctr;

		details_id=shmget(IPC_PRIVATE,MAX_TRADER*sizeof(item_details),IPC_CREAT | 0666);
		ptr[i].details=(item_details *)shmat(details_id,NULL,0);

		ctr++;
	}

	for(i=0;i<MAX_ITEM;i++)
	{
		for(j=0;j<MAX_TRADER;j++)
		{
			ptr[i].details[j].trader_id=0;
			ptr[i].details[j].quantity=0;
			ptr[i].details[j].price=-0;
		}
	}

	shmdt(ptr);
}

void print_login(login *loginptr)
{
	int i;
	
	for(i=0;i<MAX_TRADER;i++)
	{
		printf("%d\t%s\t%d\n",loginptr[i].trader_id,loginptr[i].password,loginptr[i].login_status);
	}
}

void trade_status(int connsd, item_list *ptr1, item_list *ptr2)
{
	int len,trader_id,index,i,j,p,q,diff=0,flag=0,check=0;
	req_pck packet,send_packet,final_packet;
	
	len=recv(connsd,&trader_id,sizeof(int),0);
	if(len==0)
	{
		perror("recv");
		exit(0);
	}

	//searching through buy/sell queue

	//client's request index---the requirements
	for(i=0;i<MAX_ITEM;i++)
	{
		flag=0;
		for(j=0;j<MAX_TRADER;j++)
		{
			if(ptr1[i].details[j].trader_id==trader_id)
			{
				index=j;	//to make updates after transaction is complete

				packet.item_no=ptr1[i].item_no;
				packet.price=ptr1[i].details[j].price;
				packet.quantity=ptr1[i].details[j].quantity;
			
				flag=1;
				break;				
			}
		}

		if(flag==0)	//no trades requested for item i
		{
			send_packet.trader_id=0;
			len=send(connsd,&send_packet,sizeof(req_pck),0);
			if(len==0)
			{
				perror("recv");
				exit(0);
			}
		}
		else
		{
			//searching for satisfying packet			
			check=0;
			for(p=0;p<MAX_ITEM;p++)
			{
				if(packet.item_no==ptr2[p].item_no)
				{

					for(q=0;q<MAX_TRADER;q++)
					{
						
						if(role==BUYER)					
						{
							if(ptr2[p].details[q].quantity!=0 && (ptr2[p].details[q].price <= packet.price))
							{
							//condition satisfied..perform transcation and  update values
							diff=packet.quantity-ptr2[p].details[q].quantity;
	
							if(diff<0)
							{
								ptr2[p].details[q].quantity=(-1)*diff;
								packet.quantity=0;	
							}
							else
							{
								packet.quantity=diff;
								ptr2[p].details[q].quantity=0;
							}

							final_packet.quantity=ptr1[i].details[index].quantity - packet.quantity;
							final_packet.trader_id=ptr2[p].details[q].trader_id;
							final_packet.price=ptr2[p].details[q].price;
							final_packet.item_no=packet.item_no;
							ptr1[i].details[index].quantity=packet.quantity;

							len=send(connsd,&final_packet,sizeof(req_pck),0);
							if(len==0)
							{
								perror("recv");
								exit(0);
							}

							check=1;
							break;
							}	
						}
						else if(role=SELLER)
						{
							if(ptr2[p].details[q].quantity!=0 && (ptr2[p].details[q].price >= packet.price))
							{
							//condition satisfied..perform transcation and  update values
							diff=packet.quantity-ptr2[p].details[q].quantity;
	
							if(diff<0)
							{
								ptr2[p].details[q].quantity=(-1)*diff;
								packet.quantity=0;	
							}
							else
							{
								packet.quantity=diff;
								ptr2[p].details[q].quantity=0;
							}

							final_packet.quantity=ptr1[i].details[index].quantity - packet.quantity;
							final_packet.trader_id=ptr2[p].details[q].trader_id;
							final_packet.price=ptr2[p].details[q].price;
							final_packet.item_no=packet.item_no;
							ptr1[i].details[index].quantity=packet.quantity;

							len=send(connsd,&final_packet,sizeof(req_pck),0);
							if(len==0)
							{
								perror("recv");
								exit(0);
							}

							check=1;
							break;
							}	
						}	
					}

				}
			}

			if(check==0)
			{
				send_packet.trader_id=0;
				len=send(connsd,&send_packet,sizeof(req_pck),0);
				if(len==0)
				{
					perror("recv");
					exit(0);
				}	
			}
		}
	}

}
