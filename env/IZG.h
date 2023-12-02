//
// Created by root on 21. 1. 4..
//

#ifndef ZOAD_IZG_H
#define ZOAD_IZG_H

#include <vector>
#include <queue>

#define Identify_IZG_Threshold 1.5

class IZG{
private:
    std::queue<unsigned int> *zone_queue;
    unsigned int total_zone;
    unsigned int alloc;
    std::vector<unsigned int> alloc_map;
    std::vector<unsigned int> size_map;

public:
    IZG(std::queue<unsigned int> *zones);
    int get_next_zone();
};

int alloc_izg(unsigned int flags, unsigned int iolevel);
void* zone_write(void *arg);
void* zone_read(void *arg);
int format();
int Identify_HW(std::string dev_str);
float get_zone_write_time(unsigned int zone_number);
float test_zone_write_contention(unsigned int pivot_number, unsigned int needle_number);
unsigned int get_perf(unsigned int izg_size, char rw);
int Identify_IZG();
int Identify_IO_table();
#endif //ZOAD_IZG_H
