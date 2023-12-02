## ZenFS+
[ZenFS+: Nurturing Performance and Isolation to ZenFS](https://ieeexplore.ieee.org/document/10070767)

ZenFS+ is an expansion based on [ZenFS](https://github.com/westerndigitalcorporation/zenfs).<br/>
ZenFS+ has IZ identification for identifying independently operable zones groups or Independent Zone Group (IZGs). Zones contained in an IZG are not interfered by zones in other IZGs. Based on the IZG information, ZenFS+ isolates the performance of flush/compaction operations and improves the performance by sstable striping.<br/>
ZenFS+ proactively triggers zone GC every 10 seconds. we search zones in L0 first and then find a zone that has all invalid data. Then, we just send the zone_reset command without copying. This is called as the minor GC.
For the Major GC, ZenFS+ scans all zones ranging from L0 to Lmax and selects candidate zones using the greedy policy selecting lower zones first. Major GC activates once a certain SSD capacity is utilized.
<p align="center">
  <img src="https://github.com/DKU-StarLab/ZenFS_Plus/assets/33346081/df034780-8a73-4151-97a6-332fd60a4eb2" width="30%" height="30%">
</p>
We implemented ZenFS+ in a 2TB ZNS SSD environment with 29,172 Zones, each having a Zone size of 72MB.<br/>
ZenFS+ functions in a Small-zone ZNS SSD setting, where each Zone is directly mapped to a internal parallel unit within the SSD. 

## How to build
you can build it using `make -j2 db_bench`<br/>
```
git clone https://github.com/DKU-StarLab/ZenFS-.git
cd ZenFS-
make -j2 db_bench
```
## Performance measurement
The default path for the ZNS SSD is set to **/dev/nvme0n1**, and the default environment for RocksDB is configured as ZenFS+.<br/>
```
./db_bench --benchmarks=fillrandom --use_direct_io_for_flush_and_compaction=1 --num=1000 --key_size=20 --value_size=800
```
