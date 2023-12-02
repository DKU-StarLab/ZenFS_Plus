//
// Created by root on 21. 1. 4..
//

#ifndef ZOAD_ZONE_H
#define ZOAD_ZONE_H

#define THREAD_PER_IZG 8
#include "controller.h"
#include <vector>
#include <queue>

class zone_request{
public:
    const char* w_buf;
    char* r_buf;
    std::vector<class Interval*>::iterator interval_it;
    unsigned int read_offset;
    std::vector<unsigned int>::iterator write_size_it;
    std::vector<unsigned int>::iterator sorted_write_size_it;
    unsigned int write_zone_cnt;
    unsigned int total_write_size;
    unsigned int izg_cnt;
    unsigned int actual_io_size;

    zone_request(){}
    /*
    zone_request(const char* _buf, std::vector<class Interval*>::iterator _interval_it, std::vector<unsigned int>::iterator _write_size_it, unsigned int _write_zone_cnt, unsigned int _total_write_size, std::vector<unsigned int>::iterator _sorted_write_size_it, unsigned int _izg_cnt){
        w_buf = _buf;
        write_size_it = _write_size_it;
        sorted_write_size_it = _sorted_write_size_it;
        interval_it = _interval_it;
        write_zone_cnt = _write_zone_cnt;
        total_write_size = _total_write_size;
	izg_cnt = _izg_cnt;
    }
    */
    int set_request(const char* _buf, std::vector<class Interval*>::iterator _interval_it, std::vector<unsigned int>::iterator _write_size_it, unsigned int _write_zone_cnt, unsigned int _total_write_size, std::vector<unsigned int>::iterator _sorted_write_size_it, unsigned int _izg_cnt){
        w_buf = _buf;
        write_size_it = _write_size_it;
        sorted_write_size_it = _sorted_write_size_it;
        interval_it = _interval_it;
        write_zone_cnt = _write_zone_cnt;
        total_write_size = _total_write_size;
	izg_cnt = _izg_cnt;

        return 0;
    }
    int set_request(char* _buf, std::vector<class Interval*>::iterator _interval_it, unsigned int _read_zone_cnt, unsigned int _read_offset, unsigned int _total_read_size, std::vector<unsigned int>::iterator _sorted_read_size_it, unsigned int actual_read_size){
        r_buf = _buf;
        interval_it = _interval_it;
        read_offset = _read_offset;
        write_zone_cnt = _read_zone_cnt;
        total_write_size = _total_read_size;
        sorted_write_size_it = _sorted_read_size_it;
	actual_io_size = actual_read_size;

        return 0;
    }
    int set_request(char* _buf, std::vector<class Interval*>::iterator _interval_it, unsigned int _read_zone_cnt, unsigned int _total_read_size, std::vector<unsigned int>::iterator _thread_read_size_it){
        r_buf = _buf;
        interval_it = _interval_it;
        write_zone_cnt = _read_zone_cnt;
        total_write_size = _total_read_size;
	write_size_it = _thread_read_size_it;
        return 0;
    }
};

class Interval
{
public:
    unsigned int zone_number;
    unsigned int segment_id;
    unsigned int start;
    unsigned int end;
    bool valid;

    Interval(unsigned int _zone_number, unsigned int _segment_id, unsigned int _start, unsigned int _end)
    {
        zone_number = _zone_number;
        segment_id = _segment_id;
        start = _start;
        end = _end;
        valid = true;
    }
    unsigned int get_size(){
        if(start == end){
            return 0;
        }

        return (end - start);
    }
    int invalid(){
        valid = false;
        return 0;
    }
};

class Zone
{
private:
    unsigned int number;
    std::vector<Interval*> interval_tree;
public:
    Zone(unsigned int zone_number)
    {
        number = zone_number;
    }
    bool full();
    unsigned int get_zone_number();
    unsigned int get_wp();
    unsigned int get_free_size();
    int insert_interval(Interval* interval);
};

int izg_write(const char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, std::vector<unsigned int>* write_size_vector, unsigned int write_size);
int izg_read(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, unsigned int read_size);
int izg_read(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, unsigned int offset, unsigned int read_size);
void* zone_write(void *arg);
void* zone_read(void *arg);
#endif //ZOAD_ZONE_H
