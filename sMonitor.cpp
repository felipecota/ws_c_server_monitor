    // Version 1.0
    // CPU Usage code from https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    
    #include <unistd.h>
    #include <string.h>
    #include <netinet/in.h>
    #include <openssl/sha.h>
    #include <openssl/hmac.h>
    #include <openssl/buffer.h>
    #include <sstream> 

    #include "sFuncs.cpp"

    int sockfd, client, portno; 
    int port = 8082;

    void error(const char *msg)
    {
        perror(msg);
        exit(1);
    }
    
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
    
    void waitclient () {     
        struct sockaddr_in cli_addr;
        socklen_t clilen;
        char buffer[1024];
        int n;        

        clilen = sizeof(cli_addr);
        //std::cout << "Waiting for client \n";                        
        client = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);        
        if (client < 0) 
            error("ERROR on accept");        
        bzero(buffer,1024);
        n = read(client,buffer,1023);
        if (n < 0) 
            error("ERROR reading from socket");
        //std::cout << "Client connected \n";        
        std::string str(buffer);
        std::string key = str.substr(str.find("Sec-WebSocket-Key")+19);
        key = key.substr(0, key.find("Sec-WebSocket-Extensions")-2) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";        
        char ibuf[key.length()];
        strcpy(ibuf, key.c_str());
        unsigned char obuf[20];
        SHA1((unsigned char *)ibuf, strlen(ibuf), obuf);

        char * b64(base64(obuf,20));        
        char * hs = (char*)"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
        char * fim = (char*)"\r\n\r\n";        
        char resp[0];
        strcpy(resp, hs);        
        strcat(resp, b64);        
        strcat(resp, fim);                
        n = write(client,resp,strlen(resp));        
              
        if (n < 0) 
            error("ERROR writing to socket");
        //else
            //escuta();
        //std::cout << "n:" << n << "\n";
    }    

    int main()    
    {
        struct sockaddr_in serv_addr;     

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) 
            error("ERROR opening socket");
        bzero((char *) &serv_addr, sizeof(serv_addr));

        int enable = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
        //fcntl(sockfd, F_SETFL, O_NONBLOCK); 

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);
    
        if (bind(sockfd, (struct sockaddr *) &serv_addr,
                    sizeof(serv_addr)) < 0) 
                    error("ERROR on binding");
        listen(sockfd,5);   
        int n, p, i = 0; 

        // Get first values
        getCPU();
        getNetwork(0);
        
        for (;;) {
            // Wait 1 second to read again
            // Data on client will be refreshed every 1 second too
            sleep(1);             
            if (client > 0) {
                //std::cout << "Sending to client - " << client << " \n";                                     
                std::ostringstream oss;                
                oss << "{\"p\":" << getCPU() << ",\"ml\":" << getRAMAvailable() << ",\"mt\":" << getRAMTotal() << ",\"re\":" << getNetwork(2) << ",\"rr\":" << getNetwork(1) << "}";                                   
                char * ret = new char [oss.str().length()+1];
                strcpy(ret, oss.str().c_str());

                char resp[] = {' ', ' ', '\0'};
                resp[0] = 0x81;
                resp[1] = strlen(ret);
                strcat(resp, ret);   
                //std::cout << ret << "\n";                
                n = send(client,resp,strlen(resp),MSG_NOSIGNAL); 
                if (n < 0) {
                    //std::cout << "Client disconnected\n";
                    client = 0;
                };  
                i++;
            } else {
                waitclient();
            }
        }

        return 0;
    }
