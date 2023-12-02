#include <string.h>

#include <iostream>
#include <limits>
#include <unordered_map>
#include <map>
#include <string>
#include <list>
#include <vector>
#include <algorithm>

#include <libzbc/zbc.h>
#define ZONEDEV "/dev/sdb1"
#define ZNS_UNIT_SIZE 512
#define SEGMENT_ID_INTERVAL 100000

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <set>
#include <thread>
#include <type_traits>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/posix_logger.h"
#include "util/env_posix_test_helper.h"
#include "util/mutexlock.h"

namespace leveldb {

	namespace {

		unsigned int nr_zones;
		unsigned int global_file_id = 0;

		static int open_read_only_file_limit = -1;
		//constexpr const size_t kWritableFileBufferSize = 65536;
		constexpr const size_t kWritableFileBufferSize = 0;//512;

		using namespace std;

		static Status PosixError(const std::string& context, int err_number) {
			if (err_number == ENOENT) {
				return Status::NotFound(context, strerror(err_number));
			} else {
				return Status::IOError(context, strerror(err_number));
			}
		}

		/*
		   class Limiter {
		   public:
// Limit maximum number of resources to |max_acquires|.
Limiter(int max_acquires) : acquires_allowed_(max_acquires) {}

Limiter(const Limiter&) = delete;
Limiter operator=(const Limiter&) = delete;

// If another resource is available, acquire it and return true.
// Else return false.
bool Acquire() {
int old_acquires_allowed =
acquires_allowed_.fetch_sub(1, std::memory_order_relaxed);

if (old_acquires_allowed > 0)
return true;

acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
return false;
}

// Release a resource acquired by a previous call to Acquire() that returned
// true.
void Release() {
acquires_allowed_.fetch_add(1, std::memory_order_relaxed);
}

private:
// The number of available resources.
//
// This is a counter and is not tied to the invariants of any other class, so
// it can be operated on safely using std::memory_order_relaxed.
std::atomic<int> acquires_allowed_;
};
*/

//unsigned int sync_num = 0;

class LRUCache
{
	public:
		LRUCache(unsigned int n)
		{
			size = n;
		}

		int refer(unsigned int x)
		{
			MutexLock lock(&refs_mutex_);
			// not present in cache
			if (ma.find(x) == ma.end()) {
				// cache is full
				if (dq.size() == size) {
					// delete least recently used element
					int last = dq.back();

					// Pops the last elmeent
					dq.pop_back();

					// Erase the last
					ma.erase(last);

					return last;
				}
			}
			else
			{
				// present in cache
				dq.erase(ma[x]);
			}
			// update reference
			dq.push_front(x);
			ma[x] = dq.begin();

			return -1;
		}

		unsigned int get_last()
		{
			//HOON
			return 0;
		}

	private:
		port::Mutex refs_mutex_;
		list<unsigned int> dq;
		unordered_map<unsigned int, list<unsigned int>::iterator> ma;
		unsigned int size;
};

class Interval
{
	public:
		Interval(unsigned int start, unsigned int end)
		{
			start_sector = start;
			end_sector = end;
		}

		unsigned int start_sector;
		unsigned int end_sector;
}; 


port::Mutex zone_mutex_;

class Zone
{
	public:
		Zone(unsigned int zone_number, unsigned long long init_wp) : number(zone_number), wp(init_wp)
	{
		segment_id = SEGMENT_ID_INTERVAL * zone_number;
		opened = false;
		alloc = false;
	}

		unsigned int Write(struct zbc_device_info* info, struct zbc_device *dev, unsigned int* segment_id, const char* data, size_t size)
		{//get prev segment_id and sum interval
			unsigned int next_segment_id = get_next_segment_id();
			void *iobuf = NULL;
			unsigned int sector_count = 0;
			ssize_t ret = 0;

			*segment_id = next_segment_id;
			sector_count = size/ZNS_UNIT_SIZE;
			if((size % ZNS_UNIT_SIZE))
				sector_count++;

			ret = posix_memalign((void **) &iobuf, info -> zbd_lblock_size, size);
			if (ret != 0) 
			{
				fprintf(stderr, "[ERROR],No memory for I/O buffer (%zu B)\n", size);
				return 0;
			}
			memset(iobuf, 0, size);
			memcpy(iobuf, data, size);

			MutexLock lock(&zone_mutex_);//HOON
			ret = zbc_pwrite(dev, iobuf, sector_count, wp);
			
			auto interval = new Interval(wp, wp + sector_count);
			segment_tree.insert(pair<unsigned int, Interval*>(next_segment_id, interval));
			wp += sector_count;

			/*
			cout << "Write---------" <<endl;
			cout << "size : " << size << " wp : " << wp << " sector_cnt : " << sector_count << endl;
			cout << data << endl;
			cout << "---------" <<endl;
			*/
			free(iobuf);

			return size;
		}

		size_t Read(struct zbc_device_info* info, struct zbc_device *dev, unsigned int segment_id, char* buf, unsigned int offset, unsigned int size)
		{
			void *iobuf = NULL;
			unsigned int sector_count = 0;
			ssize_t ret = 0;

			Interval* segment_interval = segment_tree.find(segment_id) -> second;

			if(size % ZNS_UNIT_SIZE)
			{
				unsigned int buf_size = size + (ZNS_UNIT_SIZE - (size % ZNS_UNIT_SIZE));

				ret = posix_memalign((void **) &iobuf, ZNS_UNIT_SIZE, buf_size);
				if (ret != 0)
				{
					fprintf(stderr, "[TEST][ERROR],No memory for I/O buffer (%zu B)\n", size);
					exit(0);
				}

				ret = zbc_pread(dev, iobuf, segment_interval -> end_sector - segment_interval -> start_sector, segment_interval -> start_sector);
				memcpy(buf, iobuf + offset, size);
				free(iobuf);
			}
			else
				ret = zbc_pread(dev, buf, segment_interval -> end_sector - segment_interval -> start_sector, segment_interval -> start_sector);

			//ret == read size(ZNS_UNIT_SIZE)

			return size;
		}

		bool Delete(unsigned int segment_id)
		{
			Interval* ptr = segment_tree.find(segment_id) -> second;
			delete ptr;
			segment_tree.erase(segment_id);

			return true;
		}

		//bool Update_interval(unsigned int segment_id, unsigned int flag, unsigned int offset, size_t size)
		//{}

		unsigned long long get_logical_wp()
		{
			return wp;
		}

		bool opened;
		bool alloc;
	private:
		unsigned int get_next_segment_id()
		{
			if(segment_id % SEGMENT_ID_INTERVAL != SEGMENT_ID_INTERVAL - 1)
			{
				unsigned int r = segment_id;
				segment_id++;

				return r;
			}
			else
			{
				segment_id += nr_zones * SEGMENT_ID_INTERVAL - (segment_id % SEGMENT_ID_INTERVAL);

				return segment_id;
			}

			//if(<max)
		}

		unsigned int number;
		unsigned long long wp;
		std::map <unsigned int, Interval*> segment_tree;
		unsigned int segment_id;
		//global buffer
};

class Z_interface {
	public:
		Z_interface()
		{
			Zone_init();

			for (unsigned int i = 0; i < nr_zones; i++)
			{
				zone_list.push_back(Zone(i, zbc_zone_wp(&zones[i])));
			}

		}

		size_t Write(unsigned int symbol, unsigned int* segment_id, const char* data, size_t size)
		{
			unsigned int count;
			unsigned int zone_number;
			unsigned int start_zone_number;

			count = symbol_map.count(symbol);

			if(count == 0)
			{
				zone_number = get_next_zone();
				symbol_map.insert(pair<unsigned int, unsigned int>(symbol, zone_number));
			}
			else
			{
				auto iter = symbol_map.lower_bound(symbol);

				zone_number = iter -> second;
				if(Check_zone_max(zone_number))
				{
					for(unsigned int i = 0; i < count - 1; i++)
					{
						iter++;
						zone_number = iter -> second;
						if(!Check_zone_max(zone_number))
							break;
					}
				}
			}

			if(Check_zone_max(zone_number))
			{
				zone_number = get_next_zone();
				symbol_map.insert(pair<unsigned int, unsigned int>(symbol, zone_number));
			}

			//HOON
			//size split zone
			opened_zone -> refer(zone_number);

			return zone_list[zone_number].Write(&info, dev, segment_id, data, size);
		}

		size_t Read(unsigned int segment_id, char* buf, unsigned int start_offset, unsigned int size)
		{
			unsigned int zone_number = (segment_id / SEGMENT_ID_INTERVAL) % nr_zones;

			opened_zone -> refer(zone_number);

			return zone_list[zone_number].Read(&info, dev, segment_id, buf, start_offset, size);
		}

		bool Delete()
		{
			return true;
		}

		bool Sync()
		{
			enum zbc_reporting_options ro = ZBC_RO_ALL;
			int ret = 1;
			uint64_t lba = 0;

			MutexLock lock(&zone_mutex_);//HOON
			ret = zbc_flush(dev);
			if(!ret)
			{
				ret = zbc_report_zones(dev, zbc_lba2sect(&info, lba), ro, zones, &nr_zones);

				for(unsigned int zone_number = 0; zone_number < nr_zones; zone_number++)
				{
					ret = Check_wp_sync(zone_number);
					if(!ret)
						return false;
				}

				return true;
			}
			else
				return false;

			return true;
		}

		~Z_interface()
		{
			//close all zones
			/* Close device file */
			zbc_close(dev);
		}
	private:
		void Zone_init()
		{
			enum zbc_reporting_options ro = ZBC_RO_ALL;
			int ret = 1;
			uint64_t lba = 0;
			struct zbc_zone *z = NULL;
			unsigned int oflags;

			/* Open device */
			//oflags = ZBC_O_DEVTEST;
			/*
			   oflags |= ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE;
			   if (!getenv("ZBC_TEST_FORCE_ATA"))
			   oflags |= ZBC_O_DRV_SCSI;

			   ret = zbc_open(ZONEDEV, oflags | O_RDONLY, &dev);
			   */

			ret = zbc_open(ZONEDEV, O_RDWR, &dev);

			zbc_get_device_info(dev, &info);

			opened_zone = new LRUCache(info.zbd_max_nr_open_seq_req);

			/* Get the number of zones */
			ret = zbc_report_nr_zones(dev, zbc_lba2sect(&info, lba), ro, &nr_zones);

			/* Allocate zone array */
			zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));

			/* Get zone information */
			ret = zbc_report_zones(dev, zbc_lba2sect(&info, lba), ro, zones, &nr_zones);
		}

		bool Open_zone(unsigned int zone_number)
		{
			unsigned int flags = 0;

			auto ret = zbc_open_zone(dev, zones[zone_number].zbz_start, flags);
			return true;
		}

		bool Close_zone(unsigned int zone_number)
		{
			unsigned int flags = 0;

			auto ret = zbc_close_zone(dev, zones[zone_number].zbz_start, flags);
			return true;
		}

		bool Check_wp_sync(unsigned int zone_number)
		{
			/*
			cout << "----------" << endl;
			cout << sync_num << endl;
			cout << zone_list[0].get_logical_wp() << endl;
			cout << zbc_zone_wp(&zones[0]) << endl;
			cout << "==========" << endl;
			sync_num++;
			*/

			if(zone_list[zone_number].get_logical_wp() == zbc_zone_wp(&zones[zone_number]))
				return true;
			else
				return false;
		}

		bool Check_zone_max(unsigned int zone_number)
		{
			//zbc_report_zones();
			//zbc_zone_full(z);

			return (zone_list[zone_number].get_logical_wp() == (zones[zone_number].zbz_start + zones[zone_number].zbz_length - 1));
		}

		unsigned int get_next_zone()
		{
			int ret = 0;

			for (unsigned int i = 0; i < nr_zones; i++)
			{
				if(!zone_list[i].opened and !zone_list[i].alloc)
				{
					ret = opened_zone -> refer(i);

					if(ret >= 0)
					{
						Close_zone(ret);
						zone_list[ret].opened = false;
					}
					Open_zone(i);
					zone_list[i].opened = true;
					zone_list[i].alloc = true;

					return i;
				}
			}
			//HOON wait GC()
			//retry
		}

		void GC()
		{
			//HOON
			//symbolmap erase
			//zone_list[i] -> alloc = false;
			//get_next_zone()
			//move valid data
		}

		LRUCache *opened_zone;
		struct zbc_zone *zones;//zlibc zone
		vector<Zone> zone_list;
		std::multimap<unsigned int, unsigned int> symbol_map;

		struct zbc_device_info info;
		struct zbc_device *dev;
};

Z_interface zone_interface = Z_interface();

class Segment
{
	public:
		Segment(unsigned int id, unsigned int offset, unsigned int size)
		{
			id_ = id;
			offset_ = offset;
			size_ = size;
		}

		unsigned int id_;
		unsigned int offset_;
		unsigned int size_;

		bool operator<(const Segment &seg) const {
			return (offset_ < seg.offset_);
		}

		static bool comp(const Segment *seg1, const Segment *seg2) {
			return (seg1->offset_ < seg2->offset_);
		}

		unsigned int get_next_offset()
		{
			return offset_ + size_;
		}
};	

void insert_segment(vector<Segment> &cont, Segment segment )
{
	auto it = upper_bound(cont.begin(), cont.end(), segment);
	cont.insert(it, segment);
}

class ZoneFile
{
	public:
		ZoneFile(uint64_t level, string filename) : symbol(level), size(0), is_manifest_(IsManifest(filename))
	{
		file_id = global_file_id;
		global_file_id++;

		buf_pos_ = 0;
	}
		// Increase the reference count.
		void Ref()
		{
			MutexLock lock(&refs_mutex_);
			++refs_;
		}

		// Decrease the reference count. Delete if this is the last reference.
		void Unref()
		{
			bool do_delete = false;
			{
				MutexLock lock(&refs_mutex_);
				--refs_;
				//HOON
				//assert(refs_ >= 0);
				if (refs_ <= 0)
				{
					do_delete = true;
				}
			}

			if (do_delete)
			{
				//HOON
				//Delete Zone's Interval
				//Delete Interval list
				//delete this;
			}
		}

		uint64_t Size() { return size; }

		Status Read(uint64_t offset, size_t n, Slice* result, char* scratch)
		{
			if (offset > size)
			{
				return Status::IOError("Offset greater than file size.");
			}

			size_t remain = n;
			unsigned int start_offset = 0;
			unsigned int read_size = 0;
			unsigned int segment_id = 0;
			unsigned int segment_size = 0;
			unsigned int segment_offset = 0;
			char* tmp_result = scratch;
			ssize_t r = 0;

			for (auto iter = file_segment.begin(); iter != file_segment.end(); ++iter)
			{
				if(!remain)
					break;

				segment_id = iter->id_;
				segment_size = iter->size_;
				segment_offset = iter->offset_;

				if(offset > segment_offset + segment_size - 1)
					continue;
				if(offset >= segment_offset and offset < segment_offset + segment_size)
				{
					if(offset == segment_offset)
					{
						start_offset = 0;
						if(remain > segment_size)
							read_size = segment_size;
						else
							read_size = remain;
					}
					else
					{
						start_offset = offset - segment_offset;
						if(start_offset + remain > segment_size)
							read_size = segment_size - start_offset;
						else
							read_size = remain;
					}

					remain -= read_size;
					offset += read_size;

					r += zone_interface.Read(segment_id, tmp_result, start_offset, read_size); //need (file id queue)
					tmp_result += read_size;
				}
			}

			*result = Slice(scratch, (r < 0) ? 0 : r);

			return Status::OK();
		}

		Status Append(const Slice& data)
		{
			size_t write_size = data.size();
			const char* write_data = data.data();

			//size_t write_size = strlen(data);
			//char* write_data = data;

			size += write_size;

			// Fit as much as possible into buffer.
			size_t copy_size = std::min(write_size, kWritableFileBufferSize - buf_pos_);

			memcpy(buf_ + buf_pos_, write_data, copy_size);
			write_data += copy_size;
			write_size -= copy_size;
			buf_pos_ += copy_size;

			if (write_size == 0)
				return Status::OK();

			// Can't fit in buffer, so need to do at least one write.
			Status status = FlushBuffer();
			if (!status.ok())
				return status;

			// Small writes go to buffer, large writes are written directly.
			if (write_size < kWritableFileBufferSize) {
				memcpy(buf_, write_data, write_size);
				buf_pos_ = write_size;
				return Status::OK();
			}

			return WriteUnbuffered(write_data, write_size);
		}

		Status Sync()
		{
			Status status = SyncDirIfManifest();
			if (!status.ok()) {
				return status;
			}

			status = FlushBuffer();
			if (status.ok() && !zone_interface.Sync())
				status = PosixError("Sync() ERROR!!!", errno);

			return status;
		}

		~ZoneFile()
		{
			//Delete Radixtree Recursive
		}

		Status FlushBuffer()
		{
			Status status = WriteUnbuffered(buf_, buf_pos_);
			buf_pos_ = 0;

			return status;
		}

	private:

		Status WriteUnbuffered(const char* data, size_t wsize)
		{
			unsigned int segment_id = 0;
			unsigned int offset = 0;

			while (wsize > 0) {
				if(file_segment.empty())
					offset = 0;
				else
				{
					auto last_segment = file_segment.back();
					offset = last_segment.get_next_offset();
				}

				ssize_t write_result = zone_interface.Write(symbol, &segment_id, data, wsize);
				if(write_result <= 0)
				{
					cout << "Error" << endl;
					exit(0);
				}
				Segment tmp(segment_id, offset, wsize);
				insert_segment(file_segment, tmp);

				data += write_result;
				wsize -= write_result;
			}

			return Status::OK();
		}

		Status SyncDirIfManifest() {
			Status status;
			if (!is_manifest_)
			{
				return status;
			}

			if(!zone_interface.Sync())
				status = PosixError("SyncDir", errno);

			return status;
		}

		static bool IsManifest(const std::string& filename) {
			return Basename(filename).starts_with("MANIFEST");
		}

		static Slice Basename(const std::string& filename)
		{
			std::string::size_type separator_pos = filename.rfind('/');
			if (separator_pos == std::string::npos) {
				return Slice(filename);
			}
			assert(filename.find('/', separator_pos + 1) == std::string::npos);

			return Slice(filename.data() + separator_pos + 1,
					filename.length() - separator_pos - 1);
		}

		// No copying allowed.
		ZoneFile(const ZoneFile&);
		void operator=(const ZoneFile&);

		port::Mutex refs_mutex_;
		int refs_ GUARDED_BY(refs_mutex_);

		// The following fields are not protected by any mutex. They are only mutable
		// while the file is being written, and concurrent access is not allowed
		// to writable files.
		uint64_t file_id;
		uint64_t symbol;
		vector<Segment> file_segment;
		uint64_t size;
		char buf_[kWritableFileBufferSize];
		size_t buf_pos_;
		const bool is_manifest_;  // True if the file's name starts with MANIFEST.
		//Buffer_ptr
};

class SequentialFileImpl : public SequentialFile
{
	public:
		explicit SequentialFileImpl(ZoneFile* file) : file_(file), pos_(0)
	{
		file_->Ref();
	}

		~SequentialFileImpl()
		{
			file_->Unref();
		}

		virtual Status Read(size_t n, Slice* result, char* scratch)
		{
			Status s = file_->Read(pos_, n, result, scratch);
			if (s.ok()) {
				pos_ += result->size();
			}
			return s;
		}

		virtual Status Skip(uint64_t n)
		{
			if (pos_ > file_->Size()) {
				return Status::IOError("pos_ > file_->Size()");
			}
			const uint64_t available = file_->Size() - pos_;
			if (n > available) {
				n = available;
			}
			pos_ += n;
			return Status::OK();
		}

	private:
		ZoneFile* file_;
		uint64_t pos_;
};

class RandomAccessFileImpl : public RandomAccessFile
{
	public:
		explicit RandomAccessFileImpl(ZoneFile* file) : file_(file)
	{
		file_->Ref();
	}

		~RandomAccessFileImpl()
		{
			file_->Unref();
		}

		virtual Status Read(uint64_t offset, size_t n, Slice* result,
				char* scratch) const {
			return file_->Read(offset, n, result, scratch);
		}

	private:
		ZoneFile* file_;
};

class WritableFileImpl : public WritableFile
{
	public:
		WritableFileImpl(ZoneFile* file) : file_(file)
	{
		file_->Ref();
	}

		~WritableFileImpl()
		{
			file_->Unref();
		}

		virtual Status Append(const Slice& data) {
			return file_->Append(data);
		}

		virtual Status Close()
		{ 
			return file_->FlushBuffer();
		}

		virtual Status Flush() override {
			return file_->FlushBuffer();
		}

		virtual Status Sync()
		{
			return file_->Sync();
		}

		static std::string Dirname(const std::string& filename)
		{
			std::string::size_type separator_pos = filename.rfind('/');
			if (separator_pos == std::string::npos) {
				return std::string(".");
			}
			assert(filename.find('/', separator_pos + 1) == std::string::npos);

			return filename.substr(0, separator_pos);
		}


	private:
		ZoneFile* file_;
};

class NoOpLogger : public Logger
{
	public:
		virtual void Logv(const char* format, va_list ap) { }
};

/*
   static int LockOrUnlock(bool lock)
   {
   return 0;
   }
   */

class ZoneFileLock : public FileLock
{
	public:
		std::string name_;
};

class ZoneLockTable
{
	private:
		port::Mutex mu_;
		std::set<std::string> locked_files_ GUARDED_BY(mu_);
	public:
		bool Insert(const std::string& fname) LOCKS_EXCLUDED(mu_) {
			mu_.Lock();
			bool succeeded = locked_files_.insert(fname).second;
			mu_.Unlock();
			return succeeded;
		}
		void Remove(const std::string& fname) LOCKS_EXCLUDED(mu_) {
			mu_.Lock();
			locked_files_.erase(fname);
			mu_.Unlock();
		}
};

class ZoneEnv : public Env
{
	public:
		ZoneEnv();
		virtual ~ZoneEnv()
		{
			for (ZoneFileMap::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
				i->second->Unref();
			}
		}

		// Partial implementation of the Env interface.
		virtual Status NewSequentialFile(const std::string& fname,
				SequentialFile** result)
		{
			MutexLock lock(&mutex_);
			if (file_map_.find(fname) == file_map_.end()) {
				*result = nullptr;
				return Status::IOError(fname, "File not found");
			}

			*result = new SequentialFileImpl(file_map_[fname]);
			return Status::OK();
		}

		virtual Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result)
		{
			MutexLock lock(&mutex_);
			if (file_map_.find(fname) == file_map_.end()) {
				*result = nullptr;
				return Status::IOError(fname, "File not found");
			}

			*result = new RandomAccessFileImpl(file_map_[fname]);
			return Status::OK();
		}

		virtual Status NewWritableFile(const std::string& fname, WritableFile** result)
		{
			unsigned int level = 0; //HOON

			MutexLock lock(&mutex_);
			if (file_map_.find(fname) != file_map_.end()) {
				DeleteFileInternal(fname);
			}

			ZoneFile* file = new ZoneFile(level, fname);
			file_map_[fname] = file;

			*result = new WritableFileImpl(file);
			return Status::OK();
		}

		virtual Status NewAppendableFile(const std::string& fname, WritableFile** result)
		{
			unsigned int level = 0; //HOON
			MutexLock lock(&mutex_);
			ZoneFile** sptr = &file_map_[fname];
			ZoneFile* file = *sptr;
			if (file == nullptr) {
				file = new ZoneFile(level, fname);
				file->Ref();
			}
			*result = new WritableFileImpl(file);
			return Status::OK();
		}

		virtual bool FileExists(const std::string& fname)
		{
			MutexLock lock(&mutex_);
			return file_map_.find(fname) != file_map_.end();
		}

		virtual Status GetChildren(const std::string& dir, std::vector<std::string>* result)
		{
			MutexLock lock(&mutex_);
			result->clear();

			for (ZoneFileMap::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
				const std::string& filename = i->first;

				if (filename.size() >= dir.size() + 1 && filename[dir.size()] == '/' &&
						Slice(filename).starts_with(Slice(dir))) {
					result->push_back(filename.substr(dir.size() + 1));
				}
			}

			return Status::OK();
		}

		void DeleteFileInternal(const std::string& fname) EXCLUSIVE_LOCKS_REQUIRED(mutex_)
		{
			if (file_map_.find(fname) == file_map_.end()) {
				return;
			}

			file_map_[fname]->Unref();
			file_map_.erase(fname);
		}

		virtual Status DeleteFile(const std::string& fname)
		{
			MutexLock lock(&mutex_);
			if (file_map_.find(fname) == file_map_.end())
			{
				return Status::IOError(fname, "File not found");
			}

			DeleteFileInternal(fname);
			return Status::OK();
		}

		virtual Status CreateDir(const std::string& dirname)
		{
			return Status::OK();
		}

		virtual Status DeleteDir(const std::string& dirname)
		{
			return Status::OK();
		}

		virtual Status GetFileSize(const std::string& fname, uint64_t* file_size)
		{
			MutexLock lock(&mutex_);
			if (file_map_.find(fname) == file_map_.end())
			{
				return Status::IOError(fname, "File not found");
			}

			*file_size = file_map_[fname]->Size();
			return Status::OK();
		}

		virtual Status RenameFile(const std::string& src, const std::string& target)
		{
			MutexLock lock(&mutex_);
			if (file_map_.find(src) == file_map_.end()) {
				return Status::IOError(src, "File not found");
			}

			DeleteFileInternal(target);
			file_map_[target] = file_map_[src];
			file_map_.erase(src);
			return Status::OK();
		}

		/*
		   virtual Status LockFile(const std::string& fname, FileLock** lock) {
		 *lock = new FileLock;
		 return Status::OK();
		 }

		 virtual Status UnlockFile(FileLock* lock) {
		 delete lock;
		 return Status::OK();
		 }
		 */

		virtual Status LockFile(const std::string& fname, FileLock** lock)
		{
			*lock = nullptr;
			Status result;
			/*
			   int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
			   if (fd < 0) {
			   result = PosixError(fname, errno);
			   } else 
			   */
			if (!locks_.Insert(fname))
			{
				result = Status::IOError("lock " + fname, "already held by process");
			}
			/*
			   else if (LockOrUnlock(true) == -1)
			   {
			   result = PosixError("lock " + fname, errno);
			   locks_.Remove(fname);
			   }
			   */
			else
			{
				ZoneFileLock* my_lock = new ZoneFileLock;
				my_lock->name_ = fname;
				*lock = my_lock;
			}

			return result;
		}

		virtual Status UnlockFile(FileLock* lock)
		{
			ZoneFileLock* my_lock = reinterpret_cast<ZoneFileLock*>(lock);
			Status result;
			/*
			   if (LockOrUnlock(false) == -1) {
			   result = PosixError("unlock", errno);
			   }
			   */
			locks_.Remove(my_lock->name_);

			delete my_lock;
			return result;
		}

		virtual void Schedule(void (*background_work_function)(void* background_work_arg), void* background_work_arg)
		{
			background_work_mutex_.Lock();

			// Start the background thread, if we haven't done so already.
			if (!started_background_thread_)
			{
				started_background_thread_ = true;
				std::thread background_thread(ZoneEnv::BackgroundThreadEntryPoint, this);
				background_thread.detach();
			}

			// If the queue is empty, the background thread may be waiting for work.
			if (background_work_queue_.empty())
			{
				background_work_cv_.Signal();
			}

			background_work_queue_.emplace(background_work_function, background_work_arg);
			background_work_mutex_.Unlock();
		}

		virtual void StartThread(void (*thread_main)(void* thread_main_arg), void* thread_main_arg)
		{
			std::thread new_thread(thread_main, thread_main_arg);
			new_thread.detach();
		}

		virtual Status GetTestDirectory(std::string* path)
		{
			*path = "/test";
			return Status::OK();
		}

		virtual Status NewLogger(const std::string& fname, Logger** result)
		{
			*result = new NoOpLogger;
			return Status::OK();
		}

		virtual uint64_t NowMicros()
		{
			struct timeval tv;
			gettimeofday(&tv, nullptr);
			return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
		}

		virtual void SleepForMicroseconds(int micros)
		{
			usleep(micros);
		}

	private:
		void BackgroundThreadMain()
		{
			while (true)
			{
				background_work_mutex_.Lock();

				// Wait until there is work to be done.
				while (background_work_queue_.empty())
				{
					background_work_cv_.Wait();
				}

				assert(!background_work_queue_.empty());
				auto background_work_function = background_work_queue_.front().function;
				void* background_work_arg = background_work_queue_.front().arg;
				background_work_queue_.pop();

				background_work_mutex_.Unlock();
				background_work_function(background_work_arg);
			}
		}

		static void BackgroundThreadEntryPoint(ZoneEnv* env)
		{
			env->BackgroundThreadMain();
		}

		// Stores the work item data in a Schedule() call.
		//
		// Instances are constructed on the thread calling Schedule() and used on the
		// background thread.
		//
		// This structure is thread-safe beacuse it is immutable.
		struct BackgroundWorkItem
		{
			explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
				: function(function), arg(arg) {}

			void (* const function)(void*);
			void* const arg;
		};

		typedef std::map<std::string, ZoneFile*> ZoneFileMap;
		//Need to seperate Lock
		//HOON
		port::Mutex mutex_;
		ZoneFileMap file_map_ GUARDED_BY(mutex_);

		port::Mutex background_work_mutex_;
		port::CondVar background_work_cv_ GUARDED_BY(background_work_mutex_);
		bool started_background_thread_ GUARDED_BY(background_work_mutex_);

		std::queue<BackgroundWorkItem> background_work_queue_
			GUARDED_BY(background_work_mutex_);

		ZoneLockTable locks_;
};

ZoneEnv::ZoneEnv()
	: background_work_cv_(&background_work_mutex_),
	started_background_thread_(false){}

	// Wraps an Env instance whose destructor is never created.
	//
	// Intended usage:
	//   using PlatformSingletonEnv = SingletonEnv<PlatformEnv>;
	//   void ConfigurePosixEnv(int param) {
	//     PlatformSingletonEnv::AssertEnvNotInitialized();
	//     // set global configuration flags.
	//   }
	//   Env* Env::Default() {
	//     static PlatformSingletonEnv default_env;
	//     return default_env.env();
	//   }
	template<typename EnvType>
	class SingletonEnv
{
	public:
		SingletonEnv() {
#if !defined(NDEBUG)
			env_initialized_.store(true, std::memory_order::memory_order_relaxed);
#endif  // !defined(NDEBUG)
			static_assert(sizeof(env_storage_) >= sizeof(EnvType),
					"env_storage_ will not fit the Env");
			static_assert(alignof(decltype(env_storage_)) >= alignof(EnvType),
					"env_storage_ does not meet the Env's alignment needs");
			new (&env_storage_) EnvType();
		}
		~SingletonEnv() = default;

		SingletonEnv(const SingletonEnv&) = delete;
		SingletonEnv& operator=(const SingletonEnv&) = delete;

		Env* env() { return reinterpret_cast<Env*>(&env_storage_); }

		static void AssertEnvNotInitialized() {
#if !defined(NDEBUG)
			assert(!env_initialized_.load(std::memory_order::memory_order_relaxed));
#endif  // !defined(NDEBUG)
		}

	private:
		typename std::aligned_storage<sizeof(EnvType), alignof(EnvType)>::type
			env_storage_;
#if !defined(NDEBUG)
		static std::atomic<bool> env_initialized_;
#endif  // !defined(NDEBUG)
};

#if !defined(NDEBUG)
template<typename EnvType>
std::atomic<bool> SingletonEnv<EnvType>::env_initialized_;
#endif  // !defined(NDEBUG)

using ZoneDefaultEnv = SingletonEnv<ZoneEnv>;

}  // namespace

Env* Env::ZoneDefault() {
	static ZoneDefaultEnv env_container;
	return env_container.env();
}

}  // namespace leveldb
