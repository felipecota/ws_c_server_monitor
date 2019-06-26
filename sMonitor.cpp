//Example code: A simple server side code, which echos back the received message. 
//Handle multiple socket connections with select and fd_set on Linux  
#include <stdio.h>  
#include <string.h>   //strlen  
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>   //close  
#include <arpa/inet.h>    //close  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros  

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#include <sstream> 
    
// Functions
#include "sFuncs.cpp" 
     
#define TRUE   1  
#define FALSE  0  
#define PORT 8082

// The handshake hash key must be sent in base64
char *base64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}

void sendHandShake(int client) {

    int n;
    char buffer[1024];         

    if (client < 0) 
        perror("ERROR on accept");        
    bzero(buffer,1024);
    n = read(client,buffer,1023);
    if (n < 0) 
        perror("ERROR reading from socket");
    //std::cout << "Client connected \n";        

    // Getting the key from client
    std::string str(buffer);
    std::string key = str.substr(str.find("Sec-WebSocket-Key")+19);
    key = key.substr(0, key.find("Sec-WebSocket-Extensions")-2) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";        

    // Getting SHA1 hash from key + guid
    char ibuf[key.length()];
    strcpy(ibuf, key.c_str());
    unsigned char obuf[20];       
    SHA1((unsigned char *)ibuf, strlen(ibuf), obuf);

    // Creating Handshake message
    char * b64(base64(obuf,20));        
    char * hs = (char*)"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
    char * fim = (char*)"\r\n\r\n";        
    char resp[0];
    strcpy(resp, hs);        
    strcat(resp, b64);        
    strcat(resp, fim);
    
    // Sending handshake back to client
    n = write(client,resp,strlen(resp));                      
    if (n < 0) 
        perror("ERROR writing to socket");        
}  
     
int main(int argc , char *argv[])   
{   
    int opt = TRUE;   
    int master_socket , addrlen , new_socket , client_socket[2] ,  
          max_clients = 2 , activity, i , valread , sd, n;   
    int max_sd;   
    struct sockaddr_in address;   
         
    //char buffer[1025];  //data buffer of 1K  
         
    //set of socket descriptors  
    fd_set readfds;   
             
    //initialise all client_socket[] to 0 so not checked  
    for (i = 0; i < max_clients; i++)   
    {   
        client_socket[i] = 0;   
    }   
         
    //create a master socket  
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)   
    {   
        perror("socket failed");   
        exit(EXIT_FAILURE);   
    }   
     
    //set master socket to allow multiple connections ,  
    //this is just a good habit, it will work without this  
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,  
          sizeof(opt)) < 0 )   
    {   
        perror("setsockopt");   
        exit(EXIT_FAILURE);   
    }   
     
    //type of socket created  
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons( PORT );   
         
    //bind the socket to localhost port  
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)   
    {   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   
    //printf("Listener on port %d \n", PORT);   
         
    //try to specify maximum of 3 pending connections for the master socket  
    if (listen(master_socket, 3) < 0)   
    {   
        perror("listen");   
        exit(EXIT_FAILURE);   
    }   
         
    //accept the incoming connection  
    addrlen = sizeof(address);   
    //puts("Waiting for connections ...");   

    // Get first values
    getCPU();
    getNetwork(0);
    int ramTotal = getRAMTotal();    
         
    while(TRUE)   
    {   
        // Wait 1 second to read again
        sleep(1); 

        // Read system information to send to clients
        //std::cout << "Sending to client " << client << " - mem " << getRAMAvailable()  << " \n";                                     
        std::ostringstream oss;                
        oss << "{\"v\":1,\"p\":" << getCPU() << ",\"ml\":" << getRAMAvailable() << ",\"mt\":" << ramTotal << ",\"re\":" << getNetwork(2) << ",\"rr\":" << getNetwork(1) << ",\"d\":[\"/:" << getFreeSpace() << "\"]}";                                   
        char * ret = new char [oss.str().length()+1];
        strcpy(ret, oss.str().c_str());

        char resp[] = {' ', ' ', '\0'};
        resp[0] = 0x81;
        resp[1] = strlen(ret);
        strcat(resp, ret);           

        //clear the socket set  
        FD_ZERO(&readfds);   
     
        //add master socket to set  
        FD_SET(master_socket, &readfds);   
        max_sd = master_socket;   
             
        //add child sockets to set  
        for ( i = 0 ; i < max_clients ; i++)   
        {   
            //socket descriptor  
            sd = client_socket[i];   
                 
            //if valid socket descriptor then add to read list  
            if(sd > 0)   
                FD_SET( sd , &readfds);   
                 
            //highest file descriptor number, need it for the select function  
            if(sd > max_sd)   
                max_sd = sd;   
        }   
     
        //wait for an activity on one of the sockets , timeout is NULL ,  
        //so wait indefinitely  
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);   
       
        if ((activity < 0) && (errno!=EINTR))   
        {   
            printf("select error");   
        }   
             
        //If something happened on the master socket ,  
        //then its an incoming connection  
        if (FD_ISSET(master_socket, &readfds))   
        {   
            if ((new_socket = accept(master_socket,  
                    (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)   
            {   
                perror("accept");   
                exit(EXIT_FAILURE);   
            }   
             
            //inform user of socket number - used in send and receive commands  
            //printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs (address.sin_port));   
           
            //send new connection greeting message  
            /*if( send(new_socket, message, strlen(message), 0) != strlen(message) )   
            {   
                perror("send");   
            }*/   
                 
            //puts("Welcome message sent successfully");   

            sendHandShake(new_socket);
                 
            //add new socket to array of sockets  
            for (i = 0; i < max_clients; i++)   
            {   
                //if position is empty  
                if( client_socket[i] == 0 )   
                {   
                    client_socket[i] = new_socket;   
                    //printf("Adding to list of sockets as %d\n" , i);   
                         
                    break;   
                }   
            }   
        }   
             
        //else its some IO operation on some other socket 
        for (i = 0; i < max_clients; i++)   
        {   
            sd = client_socket[i];  
                
            if (FD_ISSET( sd , &readfds))   
            {   
                //Check if it was for closing , and also read the  
                //incoming message
                /*  
                if ((valread = read( sd , buffer, 1024)) == 0)   
                {   
                    //Somebody disconnected , get his details and print  
                    getpeername(sd , (struct sockaddr*)&address , \ 
                        (socklen_t*)&addrlen);   
                    printf("Host disconnected , ip %s , port %d \n" ,  
                          inet_ntoa(address.sin_addr) , ntohs(address.sin_port));   
                         
                    //Close the socket and mark as 0 in list for reuse  
                    close( sd );   
                    client_socket[i] = 0;   
                }   
                     
                //Echo back the message that came in  
                else 
                {   
                    //set the string terminating NULL byte on the end  
                    //of the data read  
                    buffer[valread] = '\0';   
                    send(sd , buffer , strlen(buffer) , 0 );   
                }
                */  
           
                if (sd > 0) {    
                    // Send message to the client. If error, client is disconnected            
                    n = send(sd,resp,strlen(resp),MSG_NOSIGNAL); 
                    if (n < 0) {
                        // Not getting correct port
                        //getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);                          
                        //printf("Host socket fd %i disconnected , ip %s , port %d \n", sd, inet_ntoa(address.sin_addr), ntohs(address.sin_port));                           
                        //printf("Host socket fd %i disconnected \n", sd);
                        close(sd);
                        client_socket[i] = 0;                            
                    };  
                }                
            }   
        }   
    }   
         
    return 0;   
} 