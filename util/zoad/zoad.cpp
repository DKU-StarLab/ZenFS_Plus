//
// Created by root on 21. 1. 11..
//

#include <iostream>
#include <string>
#include <ctime>
#include <vector>
#include <utility>

#include "zoad.h"

//HOON Need dynamic izg_cnt per max partition
//HOON read/write thread 1d array -> 2d array

std::mutex partition_lock;
extern unsigned int next_partition_id;
extern std::map<unsigned int, std::vector<class Partition*>*> user_to_partition_map;
extern std::map<unsigned int,std::vector<class Partition*>*> open_partition_list;

int _z_flush(class Partition* partition, std::string data_name, const char* buf, unsigned int size, unsigned int flag) {
    int ret = 0;
    class Userdata* userdata = NULL;
    unsigned int rw = 0;
    char* req_buf = new char[size];

    memcpy(req_buf, buf, size);    

    partition->lock_partition();

    if(partition->get_status() != OPEN){
        std::cout << "_z_flush : Partition is closed " << data_name << std::endl;
        ret = -1;
    }

    if(partition->userdata_exist(data_name)){
        if(flag == Z_UPDATE){
            //partition->userdata_invalid(data_name);
        }
        userdata = partition->get_userdata(data_name);
    }
    else{
        userdata = new class Userdata(partition);
        partition->new_userdata(data_name, userdata);
    }

    //auto request = new class IO_request(WRITE, buf, size, userdata);
    auto request = new class IO_request(WRITE, req_buf, size, userdata);
    partition->new_request(request);

    partition->unlock_partition();

    return ret;
}

int z_flush(unsigned int uid, unsigned int lp_id, std::string data_name, const char* buf, unsigned int size, unsigned int flag){
    int ret = -1;
    bool locked = false;

    partition_lock.lock();
    locked = true;

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        partition_lock.unlock();
	locked = false;
        std::cout << "Cannot find uid in user_list : " << uid << std::endl;
    }
    else {
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list) {
            auto id = iter->get_id();
            if (id == lp_id) {
                partition_lock.unlock();
                locked = false;
                ret = _z_flush(iter, data_name, buf, size, flag);
                break;
            }
        }
    }

    if(locked) {
        partition_lock.unlock();
    }

    if(ret < 0){
        std::cout << "z_flush - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
    }

    return ret;
}

int _z_load(class Partition* partition, std::string data_name, char* buf, unsigned int size) {
    int ret = 0;
    class Userdata* userdata = NULL;
    unsigned int rw = 0;
    bool locked = false;

    partition->lock_partition();
    locked = true;

    if(partition->get_status() != OPEN){
        std::cout << "_z_load : Partition is closed " << data_name << std::endl;
        ret = -1;
    }

    if(partition->userdata_exist(data_name)){
        userdata = partition->get_userdata(data_name);
        //auto request = new class IO_request(READ, buf, 0, size, userdata);
        //partition->new_request(request);
    	partition->unlock_partition();
	locked = false;
	/*
        while(true) {
            while (request->read_lock.test_and_set(std::memory_order_acquire));
            if(request->read_complete){
		request->read_complete = false;
                break;
            }
            request->read_lock.clear(std::memory_order_release);
        }

        delete request;
	*/
	userdata->Read(buf, size);
    }
    else{
        std::cout << "_z_load : Cannot find user data " << data_name << std::endl;
        ret = -1;
    }

    if(locked){
        partition->unlock_partition();
    }


    return ret;
}

int _z_load(class Partition* partition, std::string data_name, char* buf, unsigned int offset, unsigned int size) {
    int ret = 0;
    class Userdata* userdata = NULL;
    unsigned int rw = 0;
    bool locked = false;
    
    struct timeval sstart = {};
    struct timeval eend = {};
    float ttime = 0;

    partition->lock_partition();
    locked = true;

    if(partition->get_status() != OPEN){
        std::cout << "_z_load_random : Partition is closed " << data_name << std::endl;
        ret = -1;
    }

    if(partition->userdata_exist(data_name)){
        userdata = partition->get_userdata(data_name);
        //auto request = new class IO_request(READ_RANDOM, buf, offset, size, userdata);
        //partition->new_request(request);
	partition->unlock_partition();
    	locked = false;
    	//std::unique_lock<std::mutex> lck(request->mtx);
        
	//std::cout << "_z_load 1" << std::endl;
	/*
    	while(true) {
            while (request->read_lock.test_and_set(std::memory_order_acquire));
	     if(request->read_complete){
		request->read_complete = false;
		//std::cout << "_z_load 2" << std::endl;
                break;
            }
            request->read_lock.clear(std::memory_order_release);
        }
	*/
 	//gettimeofday(&sstart, NULL);
        //ttime = (sstart.tv_sec) + (sstart.tv_usec * 0.000001);
	//std::cout << "RR Submit Req : " << sstart.tv_sec << " " <<  sstart.tv_usec * 0.000001 << std::endl;

	//while (!request->read_complete) request->cv.wait(lck);
	//request->read_complete = false;
    	//gettimeofday(&eend, NULL);
        //delete request;

	//std::cout << "_z_load 3" << std::endl;
	userdata->Read(buf, offset, size);
    }
    else{
        std::cout << "_z_load_random : Cannot find user data " << data_name << std::endl;
        ret = -1;
    }

    if(locked) {
        partition->unlock_partition();
    }

    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << ttime << " " << data_name << " " << offset << " " << size << std::endl;

    return ret;
}

int z_load(unsigned int uid, unsigned int lp_id, std::string data_name, char* buf, unsigned int size){
    int ret = -1;
    bool locked = false;
    partition_lock.lock();
    locked = true;
    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        partition_lock.unlock();
	locked = false;
        std::cout << "z_load - Cannot find uid in user_list : " << uid << std::endl;
    }
    else {
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list) {
            auto id = iter->get_id();
            if (id == lp_id) {
                partition_lock.unlock();
                locked = false;
                ret = _z_load(iter, data_name, buf, size);
                break;
            }
        }
    }

    if(locked) {
        partition_lock.unlock();
    }

    if(ret < 0){
        std::cout << "z_load - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
    }

    return ret;
}

int z_load(unsigned int uid, unsigned int lp_id, std::string data_name, char* buf, unsigned int offset, unsigned int size){
    int ret = -1;
    bool locked = false;

    partition_lock.lock();
    locked = true;

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        partition_lock.unlock();
	locked = false;
        std::cout << "z_load_random - Cannot find uid in user_list : " << uid << std::endl;
    }
    else {
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list) {
            auto id = iter->get_id();
            if (id == lp_id) {
                partition_lock.unlock();
                locked = false;
		//std::cout << "z_load : " << data_name << std::endl;
                ret = _z_load(iter, data_name, buf, offset, size);
                break;
            }
        }
    }

    if(locked) {
        partition_lock.unlock();
    }

    if(ret < 0){
        std::cout << "z_load - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
    }

    return ret;
}

int _z_sync(class Partition* partition, std::string data_name) {
    int ret = 0;
    class Userdata* userdata = NULL;

    partition->lock_partition();

    if(partition->get_status() != OPEN){
        std::cout << "_z_sync : Partition is closed " << data_name << std::endl;
        ret = -1;
    }

    if(partition->userdata_exist(data_name)){
        partition->unlock_partition();
        userdata = partition->get_userdata(data_name);
        userdata->Sync();
    }
    else{
        partition->unlock_partition();
        std::cout << "_z_sync : Cannot find user data " << data_name << std::endl;
        ret = -1;
    }

    return ret;
}

int z_sync(unsigned int uid, unsigned int lp_id, std::string data_name){
    int ret = -1;
    bool locked = false;

    partition_lock.lock();
    locked = true;

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        partition_lock.unlock();
        std::cout << "z_sync - Cannot find uid in user_list : " << uid << std::endl;
    }
    else {
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list) {
            auto id = iter->get_id();
            if (id == lp_id) {
                partition_lock.unlock();
                locked = false;
                ret = _z_sync(iter, data_name);
                break;
            }
        }
    }

    if(locked) {
        partition_lock.unlock();
    }

    if(ret < 0){
        std::cout << "z_sync - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
    }

    return ret;
}

int _z_del(class Partition* partition, std::string data_name) {
    int ret = 0;
    class Userdata* userdata = NULL;
    unsigned int rw = 0;

    partition->lock_partition();

    if(partition->get_status() != OPEN){
        std::cout << "_z_del : Partition is closed " << data_name << std::endl;
        ret = -1;
    }

    if(partition->userdata_exist(data_name)){
        userdata = partition->get_userdata(data_name);
        partition->invalid_userdata(data_name);
        auto request = new class IO_request(DELETE, userdata);
        partition->new_request(request);
    }
    else{
        std::cout << "_z_del : Cannot find user data " << data_name << std::endl;
        ret = -1;
    }

    partition->unlock_partition();

    return ret;
}

int z_del(unsigned int uid, unsigned int lp_id, std::string data_name){
    int ret = -1;
    bool locked = false;

    partition_lock.lock();
    locked = true;

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        partition_lock.unlock();
        std::cout << "z_del - Cannot find uid in user_list : " << uid << std::endl;
    }
    else {
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list) {
            auto id = iter->get_id();
            if (id == lp_id) {
                partition_lock.unlock();
                locked = false;
                ret = _z_del(iter, data_name);
                break;
            }
        }
    }

    if(locked) {
        partition_lock.unlock();
    }

    if(ret < 0){
        std::cout << "z_del - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
    }

    return ret;
}

int z_closelp(unsigned int uid, unsigned int lp_id){
    int ret = 0;

    partition_lock.lock();

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        std::cout << "closelp - Cannot find uid in user_list : " << uid << std::endl;
        ret = -1;
    }
    else{
        ret = -1;
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list)
        {
            auto id = iter->get_id();
            if(id == lp_id)
            {
                for(auto it = open_partition_list[iter->get_mkflag()]->begin(); it < open_partition_list[iter->get_mkflag()]->end(); it++){
                    class Partition* tmp = *(it);
                    if(tmp->get_id() == lp_id){
                        open_partition_list[iter->get_mkflag()]->erase(it);
                        break;
                    }
                }
                ret = iter->get_id();
                iter->lock_partition();
                iter->close();
                iter->unlock_partition();
                break;
            }
        }

        if(ret < 0){
            std::cout << "closelp - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
        }
    }

    partition_lock.unlock();

    return ret;
}

int z_dellp(unsigned int uid, unsigned int lp_id){
    int ret = 0;

    partition_lock.lock();

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        std::cout << "dellp - Cannot find uid in user_list : " << uid << std::endl;
        ret = -1;
    }
    else{
        ret = -1;
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list)
        {
            auto id = iter->get_id();
            if(id == lp_id)
            {
                for(auto it = open_partition_list[iter->get_mkflag()]->begin(); it < open_partition_list[iter->get_mkflag()]->end(); it++){
                    class Partition* tmp = *(it);
                    if(tmp->get_id() == lp_id){
                        open_partition_list[iter->get_mkflag()]->erase(it);
                        break;
                    }
                }
                ret = iter->get_id();
                iter->lock_partition();
                iter->del();
                iter->unlock_partition();
                break;
            }
        }

        if(ret < 0){
            std::cout << "closelp - Cannot find partition : " << lp_id << ", uid : " << uid << std::endl;
        }
    }

    partition_lock.unlock();

    return ret;
}

int z_openlp(unsigned int uid, std::string lp_name, unsigned int flags){
    int ret = 0;

    partition_lock.lock();
    if(user_to_partition_map.find(uid) == user_to_partition_map.end()) {
        std::cout << "openlp - Cannot find uid in user_list : " << uid << std::endl;
        ret = -1;
    }
    else{
        ret = -1;
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list)
        {
            auto name = iter->get_name();
            if(name == lp_name)
            {
                iter->open(flags);
                open_partition_list[iter->get_mkflag()]->push_back(iter);
                ret = iter->get_id();
                break;
            }
        }

        if(ret < 0){
            std::cout << "openlp - Cannot find partition : " << lp_name << ", uid : " << uid << std::endl;
        }
    }

    partition_lock.unlock();

    return ret;
}

class Partition* _mklp(std::string lp_name, unsigned int flags, unsigned int iolevel){
    int izg_cnt;
    class Partition* tmp_partition = NULL;

    izg_cnt = alloc_izg(flags, iolevel);
    if(izg_cnt < 0)
        std::cout << "Cannot _mklp : " << flags << std::endl;
    else
        tmp_partition = new Partition(lp_name, izg_cnt, flags);

    return tmp_partition;
}

int z_mklp(unsigned int uid, std::string dev_name, std::string lp_name, unsigned int flags, unsigned int iolevel){
    int ret = -1;

    partition_lock.lock();

    if(user_to_partition_map.find(uid) == user_to_partition_map.end()){
        auto partition_list = new std::vector<class Partition*>;
        auto tmp_partition = _mklp(lp_name, flags, iolevel);
        if(tmp_partition != NULL){
            partition_list->push_back(tmp_partition);
            user_to_partition_map.insert(std::make_pair(uid, partition_list));
            ret = tmp_partition->get_id();
        }
        else {
            delete partition_list;
        }
    }
    else{
        ret = 0;
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list)
        {
            auto name = iter->get_name();
            if(name == lp_name)
            {
                std::cout << name << std::endl;
                std::cout << "ZoAD : uid [" << uid << "] Partition name [" << lp_name << "] Already exist" << std::endl;
                ret = -1;
            }
        }
        if(ret == 0)
        {
            ret = -1;
            auto tmp_partition = _mklp(lp_name, flags, iolevel);
            if(tmp_partition != NULL) {
                partition_list->push_back(tmp_partition);
                ret = tmp_partition->get_id();
            }
        }
    }

    partition_lock.unlock();

    return ret;
}

class Partition* _rmlp(unsigned int lp_id){
    int izg_cnt;
    class Partition* tmp_partition = NULL;

    //partiton->lock_partition();
/*
    izg_cnt = dealloc_izg(flags, iolevel);
    if(izg_cnt < 0)
        std::cout << "Cannot _mklp : " << flags << std::endl;
    else
        tmp_partition = new Partition(lp_name, izg_cnt, flags);
*/

    return tmp_partition;
}

unsigned int z_rmlp(unsigned int uid, std::string dev_name, unsigned int lp_id){ //gc, log
    int ret = -1;

    partition_lock.lock();
    /*
    if(user_to_partition_map.find(uid) == user_to_partition_map.end()){
        auto partition_list = new std::vector<class Partition*>;
        auto tmp_partition = _rmlp(lp_id);
        if(tmp_partition != NULL){
            partition_list->push_back(tmp_partition);
            user_to_partition_map.insert(std::make_pair(uid, partition_list));
            ret = tmp_partition->get_id();
        }
        else {
            delete partition_list;
        }
    }
    else{
        ret = 0;
        auto partition_list = user_to_partition_map[uid];
        for (auto iter:*partition_list)
        {
            auto name = iter->get_name();
            if(name == lp_name)
            {
                std::cout << name << std::endl;
                std::cout << "ZoAD : uid [" << uid << "] Partition name [" << lp_name << "] Already exist" << std::endl;
                ret = -1;
            }
        }
        if(ret == 0)
        {
            ret = -1;
            auto tmp_partition = _mklp(lp_name, flags, iolevel);
            if(tmp_partition != NULL) {
                partition_list->push_back(tmp_partition);
                ret = tmp_partition->get_id();
            }
        }
    }
    */
    partition_lock.unlock();

    return ret;
}


//void __attribute__ ((constructor)) zoad_init(){
void zoad_init(){
    std::cout << "ZoAD : Init" << std::endl;

    std::string dev("/dev/nvme0n1");
    std::cout << "Identify HW" << std::endl;
    if(Identify_HW(dev) < 0){
        std::cout << "Identify HW Error" << std::endl;
    }

    std::cout << "Identify IZG" << std::endl;
    if(Identify_IZG() < 0){
        std::cout << "Identify IZG Error" << std::endl;
    }

    /*
    std::cout << "Identify I/O Table" << std::endl;
    if(Identify_IO_table() < 0){
        std::cout << "Identify I/O Table Error" << std::endl;
        return -1;
    }
    */

    next_partition_id = 1;
    open_partition_list.insert(std::make_pair(MAX_PARTITION, new std::vector<class Partition*>));
    open_partition_list.insert(std::make_pair(QOS_PARTITION, new std::vector<class Partition*>));

    std::cout << "ZoAD : Start" << std::endl;
}
