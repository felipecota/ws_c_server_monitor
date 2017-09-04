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
    #include <future>
    #include <sstream> 

    struct sysinfo memInfo;
    static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
    static unsigned long long lastReceived, lastSent;  
    int sockfd, client, portno;  

    int getCPU(){
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

    int getNetwork(int modo){   
        FILE* file2 = fopen("/proc/net/dev", "r");
        char buf[200], ifname[20];
        unsigned long int r_bytes, t_bytes, r_packets, t_packets;
        int total;
    
        // skip first lines
        for (int i = 0; i < 2; i++) {
            fgets(buf, 200, file2);
        }
    
        while (fgets(buf, 200, file2)) {
            sscanf(buf, "%[^:]: %lu %lu %*u %*u %*u %*u %*u %*u %lu %lu",
                   ifname, &r_bytes, &r_packets, &t_bytes, &t_packets);
            r_bytes += r_bytes;
            t_bytes += t_bytes;
            //printf("%s: rbytes: %lu rpackets: %lu tbytes: %lu tpackets: %lu\n", ifname, r_bytes, r_packets, t_bytes, t_packets);
        }
        fclose(file2);     
        
        if (modo == 0) {
            total = -1;
            lastReceived = r_bytes;
            lastSent = t_bytes;            
        } else if (modo == 1) {
            total = r_bytes - lastReceived;
            //printf("modo1 %lu %llu %i \n", r_bytes, lastReceived, total);                
            lastReceived = r_bytes;                
        } else {                
            total = t_bytes - lastSent;
            //printf("modo2 %lu %llu %i \n", t_bytes, lastSent, total);                
            lastSent = t_bytes;    
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
    
    void escuta () {     
        //std::cout << "escutando\n";

        struct sockaddr_in cli_addr;
        socklen_t clilen;
        char buffer[1024];
        int n;        

        clilen = sizeof(cli_addr);
        client = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (client < 0) error("ERROR on accept");
        bzero(buffer,1024);
        n = read(client,buffer,1023);
        if (n < 0) error("ERROR reading from socket");
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
              
        if (n < 0) error("ERROR writing to socket");  
        //std::cout << "n:" << n << "\n";
    }    

    int main()    
    {
        sysinfo (&memInfo);
        long long totalPhysMem = memInfo.totalram;
        totalPhysMem *= memInfo.mem_unit;
        long long physMemUsed = memInfo.totalram - memInfo.freeram;
        physMemUsed *= memInfo.mem_unit;

        struct sockaddr_in serv_addr;     

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) 
            error("ERROR opening socket");
        bzero((char *) &serv_addr, sizeof(serv_addr));

        int enable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
            error("setsockopt(SO_REUSEADDR) failed");        

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(8081);
    
        if (bind(sockfd, (struct sockaddr *) &serv_addr,
                    sizeof(serv_addr)) < 0) 
                    error("ERROR on binding");
        listen(sockfd,5);   
        int n, p, i = 0; 

        // Inicio os contadores
        getCPU();
        getNetwork(0);
        
        for (;;) {
            //std::cout << "cliente: " << client << "\n";
            if (client > 0) {
                //std::cout << "enviando para cliente - " << i << " \n";

                // Aguardo 1 segundo para fazer a medição dos valores novos em relação aos anteriores
                // A tela também atualiza o servidor de 1 em 1 segundo
                sleep(1);                                        

                std::ostringstream oss;                
                oss << "{\"p\":" << getCPU() << ",\"ml\":" << (totalPhysMem-physMemUsed)/1024/1024 << ",\"mt\":" << totalPhysMem/1024/1024 << ",\"re\":" << getNetwork(2) << ",\"rr\":" << getNetwork(1) << "}";                                   
                char * ret = new char [oss.str().length()+1];;
                strcpy(ret, oss.str().c_str());

                char resp[] = {' ', ' ', '\0'};
                resp[0] = 0x81;
                resp[1] = strlen(ret);
                strcat(resp, ret);   
                
                n = write(client,resp,strlen(resp)); 
                i++;

                //std::cout << ret << "\n";

                if (n < 0) {
                    //std::cout << "cliente desconectou\n";
                    client = 0;
                };  
            } else {
                escuta();
            }
        }

        return 0;
    }
