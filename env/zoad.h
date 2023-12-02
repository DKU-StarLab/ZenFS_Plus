//
// Created by root on 21. 1. 11..
//

#ifndef ZOAD_ZOAD_H
#define ZOAD_ZOAD_H

#include "Partition.h"
#include "IZG.h"
#include "IO_request.h"
#include "Userdata.h"

int _z_flush(class Partition* partition, std::string data_name, char* buf, unsigned int size, unsigned int flag);
int z_flush(unsigned int uid, unsigned int lp_id, std::string data_name, char* buf, unsigned int size, unsigned int flag);
int _z_load(class Partition* partition, std::string data_name, char* buf, unsigned int size);
int z_load(unsigned int uid, unsigned int lp_id, std::string data_name, char* buf, unsigned int size);
int _z_sync(class Partition* partition, std::string data_name);
int z_sync(unsigned int uid, unsigned int lp_id, std::string data_name);
int _z_del(class Partition* partition, std::string data_name);
int z_del(unsigned int uid, unsigned int lp_id, std::string data_name);
int z_closelp(unsigned int uid, unsigned int lp_id);
int z_openlp(unsigned int uid, std::string lp_name, unsigned int flags);
class Partition* _mklp(std::string lp_name, unsigned int flags, unsigned int iolevel);
int z_mklp(unsigned int uid, std::string dev_name, std::string lp_name, unsigned int flags, unsigned int iolevel);
class Partition* _rmlp(unsigned int lp_id);
unsigned int z_rmlp(unsigned int uid, std::string dev_name, unsigned int lp_id);
void zoad_init();
#endif //ZOAD_ZOAD_H
