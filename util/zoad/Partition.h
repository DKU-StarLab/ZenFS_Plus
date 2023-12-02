//
// Created by root on 21. 1. 4..
//

#ifndef ZOAD_PARTITION_H
#define ZOAD_PARTITION_H

#define MAX_PARTITION 0x00
#define QOS_PARTITION 0x01

#define Z_ASYNC 0x00
#define Z_SYNC 0x01
#define Z_UPDATE 0x01
#define Z_APPEND 0x02

#define OPEN 0x00
#define CLOSE 0x01

#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include "IO_request.h"

class Partition{
private:
    unsigned int id;
    std::string name;
    int izg_cnt;
    unsigned int mk_flag;
    unsigned int op_flag;
    unsigned int* status;
    std::mutex* status_lock;
    std::queue<class IO_request*> *io_queue;
    std::thread thread;
    std::mutex* io_queue_lock;
    std::map<std::string, class Userdata*> userdata_map;
    std::vector<class Zone*>* zone_vector;
    class worker_arg* worker_arg;
    std::mutex partition_lock;
    char* read_buf;
public:
    Partition(std::string p_name, int izg_num, unsigned int p_flag);
    void open(unsigned int flag);
    void close();
    void del();
    std::string get_name();
    unsigned int get_id();
    unsigned int get_izg_cnt();
    unsigned int get_mkflag();
    unsigned int get_status();
    bool userdata_exist(std::string data_name);
    int invalid_userdata(std::string data_name);
    int new_userdata(std::string data_name, class Userdata* userdata);
    class Userdata* get_userdata(std::string data_name);
    int new_request(class IO_request* request);
    int alloc_zone();
    int zone_size();
    std::vector<class Zone*>::reverse_iterator get_zone_last_iter();
    std::vector<class Zone*>::reverse_iterator get_zone_reverse_first_iter();
    std::vector<class Zone*>::iterator get_zone_first_iter();
    bool zone_empty();
    void lock_partition();
    void unlock_partition();
    char* get_read_buf();

    std::mutex *mtx;
    std::condition_variable *cv;
    bool *submit;
};
#endif //ZOAD_PARTITION_H
