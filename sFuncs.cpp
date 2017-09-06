#include "stdio.h"
#include "sys/sysinfo.h"
#include <iostream>

struct sysinfo memInfo;
static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
static unsigned long long lastReceived, lastSent;  

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

int getRAMTotal() {
    sysinfo (&memInfo);
    long long totalPhysMem = memInfo.totalram;
    totalPhysMem *= memInfo.mem_unit;
    return totalPhysMem/1024/1024;    
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