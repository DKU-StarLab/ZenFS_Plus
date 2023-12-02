//#include "helpers/memenv/memenv.h"

#include <string.h>

#include <limits>
#include <map>
#include <string>
#include <vector>

#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/mutexlock.h"

#define ZoneDev /dev/sdb1
#define SEGMENT_ID_INTERVAL 100000
namespace leveldb {

	namespace {

		unsigned int nr_zones;
		unsigned int global_file_id = 0;

		static int open_read_only_file_limit = -1;
		constexpr const size_t kWritableFileBufferSize = 65536;

		void insert_segment(std::vector<unsigned int> &cont, Segment value ) {
			ints::iterator it = std::lower_bound(cont.begin(), cont.end(), Segment.offset_, std::greater<unsigned int>() ); // find proper position in descending order
			cont.insert(it, Segment); // insert before iterator it
		}
		// Helper class to limit resource usage to avoid exhaustion.
		// Currently used to limit read-only file descriptors and mmap file usage
		// so that we do not run out of file descriptors or virtual memory, or run into
		// kernel performance problems for very large databases.
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

		class Zone {
			public:
				Zone(unsigned int zone_number) : number(zone_number)
				{
					segment_id = SEGMENT_ID_INTERVAL * zone_number;
				}

				unsigned int Write(unsigned int* segment_id, size_t size)
				{//get prev segment_id and sum interval
					next_segment_id = get_next_segment_id();
					*segment_id = next_segment_id;

					start_wp = wp;
					wp += size;
					end_wp = wp;
					//need to 512B align

					interval = new Interval(start_wp, end_wp);
					segment_tree.insert(pair<unsigned int, class interval*>(next_segment_id, interval));

					pwrite();//zlibc
				}

				size_t Read(unsigned int segment_id, char* buf, unsigned int size, unsigned int offset)
				{
					class interval* ptr = segment_tree.find(segment_id);
					if(offset + size > ptr->second - ptr->first)
					{
						//Error
					}
					
					pread(buf, ptr->first, size);
				}

				bool Delete(unsigned int segment_id)
				{
					class interval* ptr = segment_tree.find(segment_id);
					delete ptr;
					segment_tree.erase(segment_id);
				}

				bool Update_interval(unsigned int segment_id, unsigned int flag, unsigned int offset, size_t size)
				{}

				unsigned long long get_logical_wp()
				{
					return wp;
				}

			private:
				unsigned int get_next_segment_id()
				{
					if(segment_id % SEGMENT_ID_INTERVAL != SEGMENT_ID_INTERVAL - 1)
					{
						r = segment_id;
						segment_id++;
						
						return r;
					}
					else
						segment_id += nr_zones * SEGMENT_ID_INTERVAL - (segment_id % SEGMENT_ID_INTERVAL);
						//if(<max)
				}

				unsigned int number;
				unsigned long long wp;
				typedef std::map <unsigned int, class Interval*> segment_tree;
				unsigned int segment_id;
				//buffer
		};

		class Z_interface {
			public:
				Z_interface()
				{
					Zone_init();
					for (i = 0; i < nr_zones; i++)
					{
						zone_list.push_back(class Zone(i));
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
					}
					else
					{
						auto iter = symbol_map.lower_bound(symbol);
						iter += (count - 1);
						zone_number = iter -> second;
					}

					if(zone_list[zone_number].Check_max())
					{
						start_zone_number = zone_number;
						while(zone_list[zone_number].Check_max())
						{
							zone_number = get_next_zone();
							if(zone_number == start_zone_number)
							{
								wait_GC;
								MAX_GC_WAIT;
							}
						}
					}
					symbol_map.insert(pair<unsigned int, unsigned int>(symbol, zone_number));

					zone_list[zone_number].Write(segment_id, size);
				}

				size_t Read(unsigned int segment_id, char* scratch, unsigned int read_size, unsigned int start_offset)
				{
					zone_number = (segment_id / SEGMENT_ID_INTERVAL) % nr_zones;
					zone_list[zone_number].Read(segment_id, scratch, read_size, start_offset);
				}

				size_t Delete()
				{}

				bool Sync()
				{}


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
					unsigned long long lba = 0;
					struct zbc_zone *z, *zones = NULL;
					unsigned int oflags;

					/* Open device */
					oflags = ZBC_O_DEVTEST;
					oflags |= ZBC_O_DRV_ATA | ZBC_O_DRV_FAKE;
					if (!getenv("ZBC_TEST_FORCE_ATA"))
						oflags |= ZBC_O_DRV_SCSI;

					ret = zbc_open(ZoneDev, oflags | O_RDONLY, &dev);

					zbc_get_device_info(dev, &info);

					/* Get the number of zones */
					ret = zbc_report_nr_zones(dev, zbc_lba2sect(&info, lba), ro, &nr_zones);

					/* Allocate zone array */
					zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));

					/* Get zone information */
					ret = zbc_report_zones(dev, lba, ro, zones, &nr_zones);

					for (i = 0; i < (int)nr_zones; i++) {
						z = &zones[i];
						if (zbc_zone_conventional(z))
							printf("[ZONE_INFO],%05d,0x%x,0x%x,%llu,%llu,N/A\n",
									i,
									zbc_zone_type(z),
									zbc_zone_condition(z),
									zbc_sect2lba(&info, zbc_zone_start(z)),
									zbc_sect2lba(&info, zbc_zone_length(z)));
						else
							printf("[ZONE_INFO],%05d,0x%x,0x%x,%llu,%llu,%llu\n",
									i,
									zbc_zone_type(z),
									zbc_zone_condition(z),
									zbc_sect2lba(&info, zbc_zone_start(z)),
									zbc_sect2lba(&info, zbc_zone_length(z)),
									zbc_sect2lba(&info, zbc_zone_wp(z)));
					}
				}

				bool Open(unsigned int zone_number)
				{
					ret = zbc_open_zone(dev, zbc_lba2sect(&info, lba), flags);
				}

				bool Close()
				{
					ret = zbc_close_zone(dev, zbc_lba2sect(&info, lba), flags);
				}

				bool Check_wp_sync()
				{}

				bool Check_zone_max()
				{}

				unsigned int get_next_zone()
				{

				}

				unsigned int max_open_zones = 0;
				unsigned int opened_zones = 0;
				opened_zone_list;

				struct zbc_zone *zones;//zlibc zone
				vector<class Zone> zone_list;
				std::multimap<unsigned int, unsigned int> symbol_map;

				struct zbc_device_info info;
				struct zbc_device *dev;
		};

		class Interval{
			public:
				Interval(unsigned int start, unsigned int end)
				{
					LBA_start = start;
					LBA_end = end;
				}

				unsigned int LBA_start;
				unsigned int LBA_end;
		}; 

		class Segment{
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

		class ZoneFile {
			public:
				ZoneFile() refs_(0), size(0), is_manifest_(IsManifest(filename))
				{
					file_id = global_file_id;
					global_file_id++;
				}

				// Increase the reference count.
				void Ref() {
					MutexLock lock(&refs_mutex_);
					++refs_;
				}

				// Decrease the reference count. Delete if this is the last reference.
				void Unref() {
					bool do_delete = false;
					{
						MutexLock lock(&refs_mutex_);
						--refs_;
						assert(refs_ >= 0);
						if (refs_ <= 0) {
							do_delete = true;
						}
					}

					if (do_delete) {
						//Delete Zone's Interval
						//Delete Interval list
						delete this;
					}
				}

				uint64_t Size() const { return size; }

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

					for (iter = file_segment.begin(); iter != file_segment.end(); ++iter)
					{
						if(!remain)
							break;

						segment_id = iter->id_;
						segment_size = iter->size_;
						segment_offset = iter->offset_;

						if(offset > segment_offset + segment_size - 1)
							continue;

						if(offset > segment_offset)
							start_offset = offset - segment_offset;
						else
							start_offset = segment_offset;

						if(remain	> segment_size)
							read_size = segment_size - start_offset;
						else
							read_size = remain;

						remain -= read_size;

						ssize_t r = Z_interface.Read(segment_id, scratch, read_size, start_offset); //need to operate file(file id queue)
					}

					*result = Slice(scratch, (r < 0) ? 0 : r);
					
					return Status::OK();
				}

				Status Append(const Slice& data) {
					size_t write_size = data.size();
					const char* write_data = data.data();

					size += write_size;

					// Fit as much as possible into buffer.
					size_t copy_size = std::min(write_size, kWritableFileBufferSize - buf_pos_);
					std::memcpy(buf_ + buf_pos_, write_data, copy_size);
					write_data += copy_size;
					write_size -= copy_size;
					buf_pos_ += copy_size;
					if (write_size == 0) {
						return Status::OK();
					}

					// Can't fit in buffer, so need to do at least one write.
					Status status = FlushBuffer();
					if (!status.ok()) {
						return status;
					}

					// Small writes go to buffer, large writes are written directly.
					if (write_size < kWritableFileBufferSize) {
						std::memcpy(buf_, write_data, write_size);
						pos_ = write_size;
						return Status::OK();
					}
					return WriteUnbuffered(write_data, write_size);
				}

				Status Sync() {
					Status status = SyncDirIfManifest();
					if (!status.ok()) {
						return status;
					}

					status = FlushBuffer();
					if (status.ok() && Zsync() != 0) {
						status = PosixError(filename_, errno);
					}
					return status;
				}

			private:
				Status FlushBuffer() {
					Status status = WriteUnbuffered(buf_, buf_pos_);
					pos_ = 0;
					return status;
				}

				Status WriteUnbuffered(const char* data, size_t size) {
					unsigned int segment_id = 0;

					while (size > 0) {
						if(file_segment.empty())
							offset = 0;
						else
							last_segment = file_segment.back();
						
						offset = last_segment.get_next_offset();

						ssize_t write_result = Z_interface.Write(&segment_id, data, size);
						
						Segment tmp(segment_id, offset, size);
						insert_segment(file_segment, tmp);

						data += write_result;
						size -= write_result;
					}
					return Status::OK();
				}

				Status SyncDirIfManifest() {
					Status status;
					if (!is_manifest_) {
						return status;
					}

					status = Z_interface.Sync();

					return status;
				}

				static bool IsManifest(const std::string& filename) {
					return Basename(filename).starts_with("MANIFEST");
				}

				~ZoneFile(){
					//Delete Radixtree Recursive
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
				vector<Segment> file_segment;
				uint64_t size;
				char buf_[kWritableFileBufferSize];
				size_t buf_pos_;
				const bool is_manifest_;  // True if the file's name starts with MANIFEST.
				//Buffer_ptr
		};


		//************************************************
		class SequentialFileImpl : public SequentialFile {
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

		class RandomAccessFileImpl : public RandomAccessFile {
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

		class WritableFileImpl : public WritableFile {
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

				static std::string Dirname(const std::string& filename) {
					std::string::size_type separator_pos = filename.rfind('/');
					if (separator_pos == std::string::npos) {
						return std::string(".");
					}
					assert(filename.find('/', separator_pos + 1) == std::string::npos);

					return filename.substr(0, separator_pos);
				}

				static Slice Basename(const std::string& filename) {
					std::string::size_type separator_pos = filename.rfind('/');
					if (separator_pos == std::string::npos) {
						return Slice(filename);
					}
					assert(filename.find('/', separator_pos + 1) == std::string::npos);

					return Slice(filename.data() + separator_pos + 1,
							filename.length() - separator_pos - 1);
				}

			private:
				ZoneFile* file_;
		};

		class NoOpLogger : public Logger {
			public:
				virtual void Logv(const char* format, va_list ap) { }
		};

		//************************************************* Posix

		// pread() based random-access
		class PosixRandomAccessFile: public RandomAccessFile {
			private:
				std::string filename_;
				bool temporary_fd_;  // If true, fd_ is -1 and we open on every read.
				int fd_;
				Limiter* limiter_;

			public:
				PosixRandomAccessFile(const std::string& fname, int fd, Limiter* limiter)
					: filename_(fname), fd_(fd), limiter_(limiter) {
						temporary_fd_ = !limiter->Acquire();
						if (temporary_fd_) {
							// Open file on every access.
							close(fd_);
							fd_ = -1;
						}
					}

				virtual ~PosixRandomAccessFile() {
					if (!temporary_fd_) {
						close(fd_);
						limiter_->Release();
					}
				}

				virtual Status Read(uint64_t offset, size_t n, Slice* result,
						char* scratch) const {
					int fd = fd_;
					if (temporary_fd_) {
						fd = open(filename_.c_str(), O_RDONLY);
						if (fd < 0) {
							return PosixError(filename_, errno);
						}
					}

					Status s;
					ssize_t r = pread(fd, scratch, n, static_cast<off_t>(offset));
					*result = Slice(scratch, (r < 0) ? 0 : r);
					if (r < 0) {
						// An error: return a non-ok status
						s = PosixError(filename_, errno);
					}
					if (temporary_fd_) {
						// Close the temporary file descriptor opened earlier.
						close(fd);
					}
					return s;
				}
		};

		//*********************************
		class ZoneEnv : public EnvWrapper {
			public:
				explicit ZoneEnv(Env* base_env) : EnvWrapper(base_env) { }

				virtual ~ZoneEnv() {
					for (ZoneFileMap::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
						i->second->Unref();
					}
				}

				// Partial implementation of the Env interface.
				virtual Status NewSequentialFile(const std::string& fname,
						SequentialFile** result) {
					MutexLock lock(&mutex_);
					if (file_map_.find(fname) == file_map_.end()) {
						*result = nullptr;
						return Status::IOError(fname, "File not found");
					}

					*result = new SequentialFileImpl(file_map_[fname]);
					return Status::OK();
				}

				virtual Status NewRandomAccessFile(const std::string& fname,
						RandomAccessFile** result) {
					MutexLock lock(&mutex_);
					if (file_map_.find(fname) == file_map_.end()) {
						*result = nullptr;
						return Status::IOError(fname, "File not found");
					}

					*result = new RandomAccessFileImpl(file_map_[fname]);
					return Status::OK();
				}

				virtual Status NewWritableFile(const std::string& fname,
						WritableFile** result) {
					MutexLock lock(&mutex_);
					if (file_map_.find(fname) != file_map_.end()) {
						DeleteFileInternal(fname);
					}

					ZoneFile* file = new ZoneFile();
					file_map_[fname] = file;

					*result = new WritableFileImpl(file);
					return Status::OK();
				}

				virtual Status NewAppendableFile(const std::string& fname,
						WritableFile** result) {
					MutexLock lock(&mutex_);
					ZoneFile** sptr = &file_map_[fname];
					ZoneFile* file = *sptr;
					if (file == nullptr) {
						file = new ZoneFile();
						file->Ref();
					}
					*result = new WritableFileImpl(file);
					return Status::OK();
				}

				virtual bool FileExists(const std::string& fname) {
					MutexLock lock(&mutex_);
					return file_map_.find(fname) != file_map_.end();
				}

				virtual Status GetChildren(const std::string& dir,
						std::vector<std::string>* result) {
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

				void DeleteFileInternal(const std::string& fname)
					EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
						if (file_map_.find(fname) == file_map_.end()) {
							return;
						}

						file_map_[fname]->Unref();
						file_map_.erase(fname);
					}

				virtual Status DeleteFile(const std::string& fname) {
					MutexLock lock(&mutex_);
					if (file_map_.find(fname) == file_map_.end()) {
						return Status::IOError(fname, "File not found");
					}

					DeleteFileInternal(fname);
					return Status::OK();
				}

				virtual Status CreateDir(const std::string& dirname) {
					return Status::OK();
				}

				virtual Status DeleteDir(const std::string& dirname) {
					return Status::OK();
				}

				virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) {
					MutexLock lock(&mutex_);
					if (file_map_.find(fname) == file_map_.end()) {
						return Status::IOError(fname, "File not found");
					}

					*file_size = file_map_[fname]->Size();
					return Status::OK();
				}

				virtual Status RenameFile(const std::string& src,
						const std::string& target) {
					MutexLock lock(&mutex_);
					if (file_map_.find(src) == file_map_.end()) {
						return Status::IOError(src, "File not found");
					}

					DeleteFileInternal(target);
					file_map_[target] = file_map_[src];
					file_map_.erase(src);
					return Status::OK();
				}

				virtual Status LockFile(const std::string& fname, FileLock** lock) {
					*lock = new FileLock;
					return Status::OK();
				}

				virtual Status UnlockFile(FileLock* lock) {
					delete lock;
					return Status::OK();
				}

				virtual Status GetTestDirectory(std::string* path) {
					*path = "/test";
					return Status::OK();
				}

				virtual Status NewLogger(const std::string& fname, Logger** result) {
					*result = new NoOpLogger;
					return Status::OK();
				}

			private:
				typedef std::map<std::string, ZoneFile*> ZoneFileMap;
				//Need to seperate Lock
				port::Mutex mutex_;
				ZoneFileMap file_map_ GUARDED_BY(mutex_);
		};

	}  // namespace


	//*******************************
	Env* NewZoneEnv(Env* base_env) {
		return new ZoneEnv(base_env);
	}

}  // namespace leveldb
