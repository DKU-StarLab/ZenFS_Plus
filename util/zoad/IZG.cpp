//
// Created by root on 21. 1. 4..
//

#include "Partition.h"
#include "IZG.h"
#include "controller.h"
#include <iostream>

std::vector<unsigned int> iotable_r;
std::vector<unsigned int> iotable_w;
std::vector<class IZG*> izg_map;
unsigned int total_izg_cnt;
unsigned int available_izg_cnt;
std::queue<class IZG*> inactive_izg_queue;

void* test_data;

extern struct zns_info *zns_info;

IZG::IZG(std::queue<unsigned int> *zones){
    zone_queue = zones;
    total_zone = zones->size();
    alloc = 0;
}
int IZG::get_next_zone(){
    int next_zone = 0;

    if(zone_queue->empty()){
        next_zone = -1;
    }
    else {
        next_zone = zone_queue->front();
        zone_queue->pop();
    }
    return next_zone;
}
int alloc_izg(unsigned int flags, unsigned int iolevel){
    int num = -1;

    if(flags == MAX_PARTITION)
        num = available_izg_cnt;
    else if(flags == QOS_PARTITION)
    {
        //I/O Level Table
        /*
        for(int i = 0; i < total_izg_cnt; i++){
            if(iotable_r[i] >= iolevel and iotable_w[i] >= iolevel) {
                num = i + 1;
                break;
           ] }
        }
        */

        num = iolevel;
        if(num < 0 or num > available_izg_cnt) {
            std::cout << "No more IZG : " << num << ", " << available_izg_cnt << std::endl;
            num = -1;
        }
        else
            available_izg_cnt -= num;
    }
    else
    {
        std::cout << "Unknown Partition Flag : " << flags << std::endl;
        num = -1;
    }
    std::cout << "alloc_izg " << num << std::endl;

    return num;
}
void* zone_write(void *arg){
    int i = 0;
    unsigned int num = *(unsigned int*)arg;

    zns_set_zone(num, MAN_OPEN);
    while(i < ((zns_info -> zonef.zsze * BLOCK_SIZE)/(_192KB))){
        zns_write(test_data, _192KB, num);
        i++;
    }
    zns_set_zone(num, MAN_CLOSE);

    pthread_exit((void *) nullptr);
}
void* zone_read(void *arg){
    int i = 0;
    unsigned int num = *(unsigned int*)arg;

    //zns_set_zone(num, MAN_OPEN);
    while(i < ((zns_info -> zonef.zsze * BLOCK_SIZE)/(_192KB))){
        zns_read(test_data, _192KB, num, (i * _192KB)/BLOCK_SIZE);
        i++;
    }
    //zns_set_zone(num, MAN_CLOSE);

    pthread_exit((void *) nullptr);
}
int format(){
    zns_format();
    zns_get_zone_desc(REPORT_ALL, REPORT_ALL_STATE, 0, 0, true);

    return  0;

}
int Identify_HW(std::string dev_str){
    int ret = -1;
    const char* dev = dev_str.c_str();

    ret = zns_get_info(dev);
    zns_get_zone_desc(REPORT_ALL, REPORT_ALL_STATE, 0, 0, true);
    
    return ret;
}
float get_zone_write_time(unsigned int zone_number){
    pthread_t thread1;
    struct timeval start = {};
    struct timeval end = {};
    int status = 0;
    float time = 0;

    gettimeofday(&start, NULL);
    pthread_create(&thread1, NULL, &zone_write, (void*)&zone_number);
    pthread_join(thread1, (void **)&status);
    gettimeofday(&end, NULL);
    time = (end.tv_sec - start.tv_sec) + ((end.tv_usec - start.tv_usec) * 0.000001);

    zns_set_zone(zone_number, MAN_RESET);

    return time;
}
float test_zone_write_contention(unsigned int pivot_number, unsigned int needle_number){
    pthread_t threads1;
    pthread_t threads2;
    struct timeval start = {};
    struct timeval end = {};
    int status = 0;
    float time = 0;

    gettimeofday(&start, NULL);
    pthread_create(&threads1, NULL, &zone_write, (void*)&pivot_number);
    pthread_create(&threads2, NULL, &zone_write, (void*)&needle_number);
    pthread_join(threads1, (void **)&status);
    pthread_join(threads2, (void **)&status);
    gettimeofday(&end, NULL);
    time = (end.tv_sec - start.tv_sec) + ((end.tv_usec - start.tv_usec) * 0.000001);

    zns_set_zone(pivot_number, MAN_RESET);
    zns_set_zone(needle_number, MAN_RESET);

    return time;
}
unsigned int get_perf(unsigned int izg_size, char rw){
    pthread_t threads[izg_size];
    struct timeval start = {};
    struct timeval end = {};
    int status = 0;
    float time = 0;
    unsigned int perf = 0;
    std::vector<unsigned int> zone_number;
    unsigned int zone_size = zns_get_zone_size()/(1024*1024);

    for(unsigned int i = 0; i < izg_size; i++)
        zone_number.push_back(i);

    gettimeofday(&start, NULL);
    for(unsigned int i = 0; i < izg_size; i++) {
        if(rw == 'W') {
            pthread_create(&threads[i], NULL, &zone_write, (void *) &zone_number[i]);
        }
        else
            pthread_create(&threads[i], NULL, &zone_read, (void *)&zone_number[i]);
    }
    for(unsigned int i = 0; i < izg_size; i++) {
        pthread_join(threads[i], (void **) &status);
    }
    gettimeofday(&end, NULL);
    time = (end.tv_sec - start.tv_sec) + ((end.tv_usec - start.tv_usec) * 0.000001);
    perf = (zone_size * izg_size)/time;

    for(unsigned int i = 0; i < izg_size; i++)
        zns_set_zone(i, MAN_RESET);

    return perf;
}
int Identify_IZG(){
    int ret = 0;
    class IZG* new_izg = NULL;
    std::queue<unsigned int> *izg_zone;
    unsigned int pivot = 0;
    unsigned int distance = 0;
    float base_time = 0;
    float measured_time = 0;
    int total_zone = zns_get_total_zone();
    test_data = malloc(_192KB);
    memset(test_data, 'O', _192KB);

    //format();
    //base_time = get_zone_write_time(pivot);
    /*
    for(unsigned int needle = pivot + 1; needle < total_zone; needle++){
        measured_time = test_zone_write_contention(pivot, needle);
        if(measured_time > base_time * Identify_IZG_Threshold)
        {
            distance = needle;
            break;
        }
    }
    */
    //total_zone = 128;
    distance = 32;
    for(unsigned int i = 0; i < distance; i++) {
        izg_zone = new std::queue<unsigned int>;
        izg_zone->push(i);
        for (unsigned int j = i + distance; j < total_zone; j += distance) {
            izg_zone->push(j);
        }
        new_izg = new IZG(izg_zone);
        izg_map.push_back(new_izg);
        inactive_izg_queue.push(new_izg);
    }

    if(distance > 0) {
        std::cout << "IZG cnt : " << distance << std::endl;
        total_izg_cnt = available_izg_cnt = distance;
    }
    else {
        std::cout << "Cannot IZG : " << distance << std::endl;
        ret = -1;
    }
    format();
    delete test_data;

    return ret;
}
/*
int Identify_IZG_fm(){
    class IZG* new_izg = NULL;
    std::vector<unsigned int> *izg_zone;
    unsigned int pivot = 0;
    unsigned int needle = 0;
    float base_time = 0;
    float measured_time = 0;
    int total_zone = zns_get_total_zone();
    std::vector<unsigned int> included_zone;
    std::vector<unsigned int>::iterator it;

    test_data = malloc(_192KB);
    memset(test_data, 'O', _192KB);
    format();
    base_time = get_zone_write_time(pivot);
    while(pivot <= total_zone)
    {
        it = std::find(included_zone.begin(), included_zone.end(), pivot);
        if(it != included_zone.end()) {
            std::cout << "Max Channel : " << pivot << std::endl;
            total_izg_cnt = pivot;
            available_izg_cnt = pivot;
            break;
        }
        included_zone.push_back(pivot);
        izg_zone = new std::queue<unsigned int>;
        izg_zone->push(pivot);
        needle = pivot + 1;
        for(unsigned int i = needle; needle < total_zone; needle++)
        {
            measured_time = test_zone_write_contention(pivot, needle);
            if(measured_time > base_time * Identify_IZG_Threshold)
            {
                included_zone.push_back(needle);
                izg_zone->push(needle);
            }
        }
        new_izg = new IZG(izg_zone);
        izg_map.push_back(new_izg);
        inactive_izg_queue.push(new_izg);
        izg_zone = NULL;
        pivot++;
    }
    format();
    free(test_data);

    return 0;
}
*/
int Identify_IO_table(){
    pthread_t threads[total_izg_cnt];
    std::vector<unsigned int> zone_number;
    int status = 0;

    test_data = malloc(_192KB);
    memset(test_data, 'H', _192KB);

    iotable_r.push_back(0);
    iotable_w.push_back(0);

    for(unsigned int i = 1; i < total_izg_cnt; i++){
        iotable_w.push_back(get_perf(i, 'W'));
    }

    for(unsigned int i = 0; i < total_izg_cnt; i++)
        zone_number.push_back(i);
    for(unsigned int i = 0; i < total_izg_cnt; i++)
        pthread_create(&threads[i], NULL, &zone_write, (void *) &zone_number[i]);
    for(unsigned int i = 0; i < total_izg_cnt; i++)
        pthread_join(threads[i], (void **) &status);

    for(unsigned int i = 1; i < total_izg_cnt; i++){
        iotable_r.push_back(get_perf(i, 'R'));
    }
    /*
    for(unsigned int i = 1; i < total_izg_cnt; i++) {
        std::cout << iotable_w[i] << std::endl;
        std::cout << iotable_r[i] << std::endl;
        std::cout << std::endl;
    }
    */
    //format();
    delete test_data;

    return 0;
}
