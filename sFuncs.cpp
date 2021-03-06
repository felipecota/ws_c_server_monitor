#include "stdio.h"
#include "sys/sysinfo.h"
#include <iostream>
#include <sys/statvfs.h>

#include "free.cpp"

struct sysinfo memInfo;
static unsigned long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
static unsigned long long lastReceived, lastSent;  

// My system uses old kernel so I'm using free.pl to get available memory.
// For new system you can use the code above
/*
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
    }
    fclose(file3);      

    return (int)value/1024;  
}
*/

int getRAMTotal() {
    sysinfo (&memInfo);
    long long totalPhysMem = memInfo.totalram;
    totalPhysMem *= memInfo.mem_unit;
    return totalPhysMem/1024/1024;    
}

// CPU Usage code from https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
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
        //printf("modo1 %lu %llu %i %i \n", total_r_bytes, lastReceived, total, total*8/1024/1024);                
        lastReceived = total_r_bytes;                
    } else {                
        total = total_t_bytes - lastSent;
        //printf("modo2 %lu %llu %i %i \n", total_t_bytes, lastSent, total, total*8/1024/1024);                
        lastSent = total_t_bytes;    
    }              

    return total*8/1024/1024;
}   

int getFreeSpace() { 
        struct statvfs fiData;
        if((statvfs("/",&fiData)) < 0 ) {
            return 0;
        } else {
            return 100.0 * (double) (fiData.f_blocks - fiData.f_bfree) / (double) (fiData.f_blocks - fiData.f_bfree + fiData.f_bavail);
        }
}