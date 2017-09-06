    // Version 1.0
    // Author: Felipe Cota (felipe_cota@hotmail.com)
    // CPU Usage code from https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    
    #include "stdio.h"
    #include <iostream>
    #include "sys/sysinfo.h"
    #include <unistd.h>
    #include <sys/socket.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h> 
    #include <netinet/in.h>
    #include <openssl/sha.h>
    #include <openssl/hmac.h>
    #include <openssl/evp.h>
    #include <openssl/bio.h>
    #include <openssl/buffer.h>
    #include <sstream> 
    #include <sys/fcntl.h>

    struct sysinfo memInfo;
    static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
    static unsigned long long lastReceived, lastSent;  
    int sockfd, client, portno;  

    int getRAMAvailable() {
        FILE* file3 = fopen("/proc/meminfo", "r");
        char buf[200], ifname[20];
        unsigned long int kbytes, value;
   
        while (fgets(buf, 200, file3)) {
        sscanf(buf, "%[^:]: %lu",
                   ifname, &kbytes);
            std::string str(ifname);
            if (str == "MemAvailable") {
                value = kbytes;
            } else if (str == "MemFree") { // Some machines don't have MemAvailable so get MemFree
                value = kbytes;
            }
        }
        fclose(file3);      

        return (int)value/1024;    
    }

    int getCPU() {
        double percent;
        FILE* file;
        unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

        file = fopen("/proc/stat", "r");
        fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
        fclose(file);

        if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
            totalSys < lastTotalSys || totalIdle < lastTotalIdle){
            //Overflow detection. Just skip this value.
            percent = -1.0;
        } else {
            total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
                (totalSys - lastTotalSys);
            percent = total;
            total += (totalIdle - lastTotalIdle);
            percent /= total;
            percent *= 100;
        }

        lastTotalUser = totalUser;
        lastTotalUserLow = totalUserLow;
        lastTotalSys = totalSys;
        lastTotalIdle = totalIdle;

        return (int)percent;
    }

    int getNetwork(int modo) {   
        FILE* file2 = fopen("/proc/net/dev", "r");
        char buf[200], ifname[20];
        unsigned long int r_bytes, t_bytes, r_packets, t_packets, total_r_bytes, total_t_bytes;
        int total;
    
        // skip first lines
        for (int i = 0; i < 2; i++) {
            fgets(buf, 200, file2);
        }

        total_r_bytes = 0;
        total_t_bytes = 0;
    
        while (fgets(buf, 200, file2)) {
            sscanf(buf, "%[^:]: %lu %lu %*u %*u %*u %*u %*u %*u %lu %lu",
                   ifname, &r_bytes, &r_packets, &t_bytes, &t_packets);
            total_r_bytes += r_bytes;
            total_t_bytes += t_bytes;
            //printf("rbytes: %lu tbytes: %lu\n", total_r_bytes, total_t_bytes);
        }
        fclose(file2);     
        
        if (modo == 0) {
            total = -1;
            lastReceived = total_r_bytes;
            lastSent = total_t_bytes;            
        } else if (modo == 1) {
            total = total_r_bytes - lastReceived;
            //printf("modo1 %lu %llu %i \n", r_bytes, lastReceived, total);                
            lastReceived = total_r_bytes;                
        } else {                
            total = total_t_bytes - lastSent;
            //printf("modo2 %lu %llu %i \n", t_bytes, lastSent, total);                
            lastSent = total_t_bytes;    
        }              

        return total/1024/1024*8;
    }    

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
        if (client < 0) error("ERROR on accept");        
        bzero(buffer,1024);
        n = read(client,buffer,1023);
        if (n < 0) error("ERROR reading from socket");
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
        sysinfo (&memInfo);
        long long totalPhysMem = memInfo.totalram;
        totalPhysMem *= memInfo.mem_unit;

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
        serv_addr.sin_port = htons(8082);
    
        if (bind(sockfd, (struct sockaddr *) &serv_addr,
                    sizeof(serv_addr)) < 0) 
                    error("ERROR on binding");
        listen(sockfd,5);   
        int n, p, i = 0; 

        // Get first values
        getCPU();
        getNetwork(0);
        //waitclient();
        
        for (;;) {
            // Wait 1 second to read again
            // Data on client will be refreshed every 1 second too
            sleep(1);             
            if (client > 0) {
                //std::cout << "Sending to client - " << client << " \n";                                     
                std::ostringstream oss;                
                oss << "{\"p\":" << getCPU() << ",\"ml\":" << getRAMAvailable() << ",\"mt\":" << totalPhysMem/1024/1024 << ",\"re\":" << getNetwork(2) << ",\"rr\":" << getNetwork(1) << "}";                                   
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
