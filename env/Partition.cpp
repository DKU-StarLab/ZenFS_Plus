//
// Created by root on 21. 1. 4..
//

#include "Partition.h"
#include "IZG.h"
#include "Userdata.h"
#include "Zone.h"
#include "IO_request.h"

unsigned int next_partition_id;
std::map<unsigned int, std::vector<class Partition*>*> user_to_partition_map;
std::map<unsigned int,std::vector<class Partition*>*> open_partition_list;

extern std::queue<class IZG*> inactive_izg_queue;

Partition::Partition(std::string p_name, int izg_num, unsigned int p_flag){
    status_lock = new std::mutex;
    io_queue = new std::queue<class IO_request*>;
    io_queue_lock = new std::mutex;
    status = new unsigned int;
    zone_vector = new std::vector<class Zone*>;
    worker_arg = new class worker_arg();
    id = next_partition_id;
    next_partition_id++;
    name = p_name;
    izg_cnt = izg_num;
    mk_flag = p_flag;
    *status = CLOSE;
}
void Partition::open(unsigned int flag){
    status_lock->lock();
    if(*status == OPEN){
        std::cout << "Already Open Zone" << std::endl;
        exit(0);
    }
    *status = OPEN;
    status_lock->unlock();
    op_flag = flag;
    worker_arg->status = status;
    worker_arg->status_lock = status_lock;
    worker_arg->io_queue = io_queue;
    worker_arg->io_queue_lock = io_queue_lock;
    thread = std::thread(worker_job, worker_arg);
}
void Partition::close(){
    status_lock->lock();
    if(*status == CLOSE){
        std::cout << "Already Close Zone" << std::endl;
        exit(0);
    }
    *status = CLOSE;
    status_lock->unlock();
    thread.join();
    delete io_queue;
    delete io_queue_lock;
    delete status;
    delete status_lock;
    delete worker_arg;
}
std::string Partition::get_name(){
    return name;
}
unsigned int Partition::get_id(){
    return id;
}
unsigned int Partition::get_izg_cnt(){
    return izg_cnt;
}
unsigned int Partition::get_mkflag(){
    return mk_flag;
}
unsigned int Partition::get_status(){
    unsigned int ret = 0;
    status_lock->lock();
    ret = *status;
    status_lock->unlock();
    return ret;
}
void Partition::lock_partition(){
    partition_lock.lock();
}
void Partition::unlock_partition(){
    partition_lock.unlock();
}
bool Partition::userdata_exist(std::string data_name){
    if(userdata_map.find(data_name) == userdata_map.end()) {
        return false;
    }
    else {
        return true;
    }
}
class Userdata* Partition::get_userdata(std::string data_name){
    auto it = userdata_map.find(data_name);
    return it->second;
}
int Partition::invalid_userdata(std::string data_name){
    userdata_map.erase(data_name);
    return 0;
}
int Partition::new_userdata(std::string data_name, class Userdata* userdata){
    userdata_map.insert(std::make_pair(data_name, userdata));
    return 0;
}
int Partition::new_request(class IO_request* request){
    io_queue_lock->lock();
    io_queue->push(request);
    //std::cout << io_queue->size() << std::endl;
    io_queue_lock->unlock();

    return 0;
}
int Partition::alloc_zone()
{
    int ret = 0;
    int next_zone = 0;
    class IZG* izg = NULL;

    //for (unsigned int i = 0; i < izg_cnt; i++) {
    izg = inactive_izg_queue.front();
    next_zone = izg->get_next_zone();
    if (next_zone < 0) {
        return -1;
    }
    inactive_izg_queue.pop();
    inactive_izg_queue.push(izg);
    auto zone = new class Zone(next_zone);
    zone_vector->push_back(zone);
    //}

    return 0;
}
std::vector<class Zone*>::iterator Partition::get_zone_last_iter(){
    return zone_vector->end();
}
std::vector<class Zone*>::iterator Partition::get_zone_first_iter(){
    return zone_vector->begin();
}
bool Partition::zone_empty(){
    return zone_vector->empty();
}
int Partition::zone_size(){
    return zone_vector->size();
}