//
// Created by root on 21. 1. 4..
//
//#include <pthread.h>
#include <iostream>
#include <thread>
#include "Zone.h"

bool Zone::full(){
    return zns_full_zone(number);
}
unsigned int Zone::get_zone_number(){
    return number;
}
unsigned int Zone::get_wp(){
    return zns_get_wp(number);
}
unsigned int Zone::get_free_size(){
    auto start_lba = zns_get_start_lba(number);
    auto wp = zns_get_wp(number);
    auto zone_size = zns_get_zone_size();

    //std::cout << start_lba << std::endl;
    //std::cout << wp << std::endl;
    //std::cout << zone_size << std::endl;

    //return (zone_size - (wp - start_lba)) * BLOCK_SIZE;
    return zone_size - (wp - start_lba);
}

int Zone::insert_interval(Interval* interval){
    interval_tree.push_back(interval);
    return 0;
}
/*
void* thread_read(void *arg){
    int i = 0;
    unsigned int num = *(unsigned int*)arg;

    //zns_set_zone(num, MAN_OPEN);
    while(i < ((zns_info -> zonef.zsze * BLOCK_SIZE)/(_192KB))){
        zns_read(test_data, IO_SIZE, num, (i * _192KB)/BLOCK_SIZE);
        i++;
    }
    //zns_set_zone(num, MAN_CLOSE);

    pthread_exit((void *) nullptr);
}
*/
void* thread_write(class zone_request* arg){
    class zone_request* req = arg;
    char* buf = req->buf;
    std::vector<class Interval*>::iterator interval_it = req->interval_it;
    std::vector<unsigned int>::iterator write_size_it = req->write_size_it;
    unsigned int write_zone_cnt = req->write_zone_cnt;
    unsigned int total_write_size = req->total_write_size;
    unsigned int total_written_size = 0;
    class Interval* tmp_interval;

    total_write_size *= IO_SIZE;
    if(total_write_size <= 0){
        //pthread_exit((void *) nullptr);
        return NULL;
    }

    for(int i = 0; i < write_zone_cnt; i++) {
        tmp_interval = *interval_it;
        zns_set_zone(tmp_interval->zone_number, MAN_OPEN);
        interval_it++;
    }

    interval_it -= write_zone_cnt;
    while (total_written_size < total_write_size)
    {
        for (int i = 0; i < write_zone_cnt; i++) {
            if(*write_size_it > 0) {
                //std::cout << tmp_interval->zone_number << std::endl;
                tmp_interval = *interval_it;
                zns_write(buf, IO_SIZE, tmp_interval->zone_number);
                *write_size_it -= 1;
                buf += IO_SIZE;
                total_written_size += IO_SIZE;

                if(*write_size_it <= 0){
                    tmp_interval->end = zns_get_wp(tmp_interval->zone_number);
                }
            }
            interval_it++;
            write_size_it++;
        }
        interval_it -= write_zone_cnt;
        write_size_it -= write_zone_cnt;
    }

    for(int i = 0; i < req->write_zone_cnt; i++) {
        tmp_interval = *interval_it;
        zns_set_zone(tmp_interval->zone_number, MAN_CLOSE);
        interval_it++;
    }

    return NULL;
}
void* thread_read(class zone_request* arg){
    class zone_request* req = arg;
    char* buf = req->buf;
    std::vector<class Interval*>::iterator interval_it = req->interval_it;
    unsigned int read_zone_cnt = req->write_zone_cnt;
    unsigned int total_read_size = req->total_write_size;
    unsigned int total_read_size2 = 0;
    class Interval* tmp_interval;
    unsigned int start[read_zone_cnt];
    unsigned int end[read_zone_cnt];

    total_read_size *= IO_SIZE;
    if(total_read_size <= 0){
        return NULL;
    }

    for(int i = 0; i < read_zone_cnt; i++) {
        tmp_interval = *interval_it;
        start[i] = tmp_interval->start;
        end[i] = tmp_interval->end;
        interval_it++;
    }
    interval_it -= read_zone_cnt;

    char* tmp_buf = buf;

    while (total_read_size2 < total_read_size)
    {
        for (int i = 0; i < read_zone_cnt; i++) {
            if(start[i] < end[i]) {
                tmp_interval = *interval_it;
                zns_read(buf, IO_SIZE, tmp_interval->zone_number, start[i]);
                //std::cout << i << " " << start[i] << " " << tmp_buf[IO_SIZE] << std::endl;
                total_read_size2 += IO_SIZE;
                buf += IO_SIZE;
                start[i] += IO_BLOCKS;
            }
            interval_it++;
        }
        interval_it -= read_zone_cnt;
    }

    return NULL;
}
int izg_write(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, std::vector<unsigned int>* write_size_vector, unsigned int write_blocks) {
    //struct timeval sstart = {};
    //struct timeval eend = {};
    //float ttime = 0;
    unsigned int thread_num = (izg_cnt / THREAD_PER_IZG);
    if(izg_cnt % THREAD_PER_IZG){
        thread_num++;
    }
    unsigned int izg_write = 0;
    std::thread thread[thread_num];
    int status = 0;
    unsigned int last_izg_each_zone_write_cnt;
    std::vector<unsigned int>::iterator write_size_it;
    std::vector<class Interval*>::iterator interval_it;
    unsigned int interval_list_size = 0;
    unsigned int write_zone_cnt = 0;
    unsigned int thread_write_size = 0;
    class zone_request* tmp_req[thread_num];

    for(int i = 0; i < thread_num; i++){
        tmp_req[i]= new class zone_request();
    }

    izg_write = total_target_zone/izg_cnt;
    if(total_target_zone % izg_cnt){
        izg_write++;
    }

    write_size_it = write_size_vector->begin();
    for (int i = 0; i < izg_write; i++) {
        if(i < izg_write - 1) {
            for (int j = 0; j < izg_cnt; j++) {
                write_blocks -= *write_size_it;
                *write_size_it = *write_size_it/IO_BLOCKS;
                write_size_it++;
            }
        }
        else{
            last_izg_each_zone_write_cnt = write_blocks / (izg_cnt * IO_BLOCKS);
            for(int j = 0; j < izg_cnt; j++){
                *write_size_it = last_izg_each_zone_write_cnt;
                write_size_it++;
            }
            for(int j = 0; j < izg_cnt; j++){
                write_size_it--;
            }

            last_izg_each_zone_write_cnt = (write_blocks % (izg_cnt * IO_BLOCKS))/IO_BLOCKS;
            for(int j = 0; j < last_izg_each_zone_write_cnt; j++){
                *write_size_it += 1;
                write_size_it++;
            }
        }
    }

    interval_it = interval_list->begin();
    interval_list_size = interval_list->size();
    write_size_it = write_size_vector->begin();
    //gettimeofday(&sstart, NULL);
    for (int i = 0; i < izg_write; i++) {
        //write_size_it = write_size_vector->begin();
        for (int j = 0; j < thread_num; j++) {
            thread_write_size = 0;
            if(izg_cnt < interval_list_size){
                write_zone_cnt = izg_cnt;
            }
            else{
                write_zone_cnt = interval_list_size;
            }
            if(THREAD_PER_IZG < write_zone_cnt){
                if(THREAD_PER_IZG * (j + 1) > izg_cnt){
                    write_zone_cnt = izg_cnt - THREAD_PER_IZG;
                }
                else{
                    write_zone_cnt = THREAD_PER_IZG;
                }
            }

            for(int k = 0; k < write_zone_cnt; k++){
                thread_write_size += *write_size_it;
                write_size_it++;
            }

            write_size_it -= write_zone_cnt;

            tmp_req[j]->set_request(buf, interval_it, write_size_it, write_zone_cnt, thread_write_size);
            thread[j] = std::thread(thread_write, tmp_req[j]);
            buf += thread_write_size * IO_SIZE;
            interval_it += write_zone_cnt;
            write_size_it += write_zone_cnt;
            interval_list_size -= write_zone_cnt;
        }

        for (int k = 0; k < thread_num; k++) {
            thread[k].join();
        }
    }
    //gettimeofday(&eend, NULL);
    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << ttime << std::endl;
    for(int i = 0; i < thread_num; i++){
        delete tmp_req[i];
    }

    return 0;
}
int izg_read(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, unsigned int read_size){
    //struct timeval sstart = {};
    //struct timeval eend = {};
    //float ttime = 0;
    unsigned int thread_num = (izg_cnt / THREAD_PER_IZG);
    if(izg_cnt % THREAD_PER_IZG){
        thread_num++;
    }
    unsigned int izg_read = 0;
    std::thread thread[thread_num];
    int status = 0;
    unsigned int last_izg_each_zone_write_cnt;
    std::vector<class Interval*>::iterator interval_it;
    unsigned int interval_list_size = 0;
    unsigned int read_zone_cnt = 0;
    unsigned int thread_read_size = 0;
    class zone_request* tmp_req[thread_num];
    class Interval* tmp_interval;
    char* tmp_buf = buf;

    for(int i = 0; i < thread_num; i++){
        tmp_req[i]= new class zone_request();
    }

    izg_read = total_target_zone/izg_cnt;
    if(total_target_zone % izg_cnt){
        izg_read++;
    }

    interval_it = interval_list->begin();
    interval_list_size = interval_list->size();

    //gettimeofday(&sstart, NULL);
    for (int i = 0; i < izg_read; i++) {
        for (int j = 0; j < thread_num; j++) {
            thread_read_size = 0;

            if(izg_cnt < interval_list_size){
                read_zone_cnt = izg_cnt;
            }
            else{
                read_zone_cnt = interval_list_size;
            }
            if(THREAD_PER_IZG < read_zone_cnt){
                if(THREAD_PER_IZG * (j + 1) > izg_cnt){
                    read_zone_cnt = izg_cnt - THREAD_PER_IZG;
                }
                else{
                    read_zone_cnt = THREAD_PER_IZG;
                }
            }
            for(int k = 0; k < read_zone_cnt; k++, interval_it++){
                tmp_interval = *interval_it;
                thread_read_size += tmp_interval->get_size();
            }
            thread_read_size /= IO_BLOCKS;
            interval_it -= read_zone_cnt;
            tmp_req[j]->set_request(tmp_buf, interval_it, read_zone_cnt, thread_read_size);
            thread[j] = std::thread(thread_read, tmp_req[j]);

            tmp_buf += thread_read_size * IO_SIZE;
            interval_it += read_zone_cnt;
            interval_list_size -= read_zone_cnt;
        }
        for (int k = 0; k < thread_num; k++) {
            thread[k].join();
        }
    }
    //gettimeofday(&eend, NULL);
    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << ttime << std::endl;
    for(int i = 0; i < thread_num; i++){
        delete tmp_req[i];
    }

    return 0;
}
