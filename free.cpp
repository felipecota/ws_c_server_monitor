#include <cstdio>
#include <iostream>
#include <vector>

// Using perl script (free.pl) from https://github.com/famzah/linux-memavailable-procfs/blob/master/Linux-MemAvailable.pm

int getRAMAvailable() {
    FILE* fp = popen("./free.pl", "r");
    if(fp) {
        std::vector<char> buffer(4096);
        //std::size_t n = fread(buffer.data(), 1, buffer.size(), fp);

        char buf[200], ifname[20];
        unsigned long int kbytes;

        while (fgets(buf, 200, fp)) {
            sscanf(buf, " -/+ avail %*d %lu",
            &kbytes);
        }

        pclose(fp);

        return kbytes/1024;
    }
}