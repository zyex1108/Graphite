#include "l1_cache_cntlr.h"
#include "l2_cache_cntlr.h" 
#include "memory_manager.h"
#include "log.h"

namespace PrL1PrL2DramDirectoryMOSI
{

L1CacheCntlr::L1CacheCntlr(MemoryManager* memory_manager,
                           Semaphore* user_thread_sem,
                           Semaphore* network_thread_sem,
                           UInt32 cache_line_size,
                           UInt32 L1_icache_size,
                           UInt32 L1_icache_associativity,
                           string L1_icache_replacement_policy,
                           UInt32 L1_icache_access_delay,
                           bool L1_icache_track_miss_types,
                           UInt32 L1_dcache_size,
                           UInt32 L1_dcache_associativity,
                           string L1_dcache_replacement_policy,
                           UInt32 L1_dcache_access_delay,
                           bool L1_dcache_track_miss_types,
                           volatile float frequency)
   : _memory_manager(memory_manager)
   , _L2_cache_cntlr(NULL)
   , _user_thread_sem(user_thread_sem)
   , _network_thread_sem(network_thread_sem)
{
   _L1_icache = new Cache("L1-I",
         L1_icache_size,
         L1_icache_associativity, 
         cache_line_size,
         L1_icache_replacement_policy,
         Cache::INSTRUCTION_CACHE,
         Cache::UNDEFINED_WRITE_POLICY,
         Cache::PR_L1_CACHE,
         L1_icache_access_delay,
         frequency,
         L1_icache_track_miss_types);
   _L1_dcache = new Cache("L1-D",
         L1_dcache_size,
         L1_dcache_associativity, 
         cache_line_size,
         L1_dcache_replacement_policy,
         Cache::DATA_CACHE,
         Cache::WRITE_THROUGH,
         Cache::PR_L1_CACHE,
         L1_dcache_access_delay,
         frequency,
         L1_dcache_track_miss_types);
}

L1CacheCntlr::~L1CacheCntlr()
{
   delete _L1_icache;
   delete _L1_dcache;
}      

void
L1CacheCntlr::setL2CacheCntlr(L2CacheCntlr* L2_cache_cntlr)
{
   _L2_cache_cntlr = L2_cache_cntlr;
}

bool
L1CacheCntlr::processMemOpFromTile(MemComponent::Type mem_component,
                                   Core::lock_signal_t lock_signal,
                                   Core::mem_op_t mem_op_type, 
                                   IntPtr ca_address, UInt32 offset,
                                   Byte* data_buf, UInt32 data_length,
                                   bool modeled)
{
   LOG_PRINT("processMemOpFromTile(), lock_signal(%u), mem_op_type(%u), ca_address(%#llx)",
             lock_signal, mem_op_type, ca_address);

   bool L1_cache_hit = true;
   UInt32 access_num = 0;

   while(1)
   {
      access_num ++;
      LOG_ASSERT_ERROR((access_num == 1) || (access_num == 2),
            "Error: access_num(%u)", access_num);

      if (lock_signal != Core::UNLOCK)
         acquireLock(mem_component);

      // Wake up the network thread after acquiring the lock
      if (access_num == 2)
      {
         wakeUpNetworkThread();
      }

      if (operationPermissibleinL1Cache(mem_component, ca_address, mem_op_type, access_num))
      {
         // Increment Shared Mem Perf model cycle counts
         // L1 Cache
         getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

         accessCache(mem_component, mem_op_type, ca_address, offset, data_buf, data_length);
                 
         if (lock_signal != Core::LOCK)
            releaseLock(mem_component);
         return L1_cache_hit;
      }

      getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_TAGS);

      // The memory request misses in the L1 cache
      L1_cache_hit = false;

      if (lock_signal == Core::UNLOCK)
         LOG_PRINT_ERROR("Expected to find address(0x%x) in L1 Cache", ca_address);

      _L2_cache_cntlr->acquireLock();

      pair<bool,Cache::MissType> L2_cache_miss_info = _L2_cache_cntlr->processShmemRequestFromL1Cache(mem_component, mem_op_type, ca_address);
      bool L2_cache_miss = L2_cache_miss_info.first;
      Cache::MissType L2_cache_miss_type = L2_cache_miss_info.second;

      if (!L2_cache_miss)
      {
         _L2_cache_cntlr->releaseLock();
         
         // Increment Shared Mem Perf model cycle counts
         // L2 Cache
         getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);
         // L1 Cache
         getMemoryManager()->incrCycleCount(mem_component, CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS);

         accessCache(mem_component, mem_op_type, ca_address, offset, data_buf, data_length);

         if (lock_signal != Core::LOCK)
            releaseLock(mem_component);
         return false;
      }

      // Increment shared mem perf model cycle counts
      getMemoryManager()->incrCycleCount(MemComponent::L2_CACHE, CachePerfModel::ACCESS_CACHE_TAGS);
      
      _L2_cache_cntlr->releaseLock();
      releaseLock(mem_component);
      
      // Send out a request to the network thread for the cache data
      bool msg_modeled = ::MemoryManager::isMissTypeModeled(L2_cache_miss_type) &&
                         Config::getSingleton()->isApplicationTile(getMemoryManager()->getTile()->getId());

      ShmemMsg::Type shmem_msg_type = getShmemMsgType(mem_op_type);
      ShmemMsg shmem_msg(shmem_msg_type, mem_component, MemComponent::L2_CACHE,
                         getTileId(), INVALID_TILE_ID, false, ca_address, msg_modeled);
      getMemoryManager()->sendMsg(getTileId(), shmem_msg);

      waitForNetworkThread();
   }

   LOG_PRINT_ERROR("Should not reach here");
   return false;
}

void
L1CacheCntlr::accessCache(MemComponent::Type mem_component,
      Core::mem_op_t mem_op_type, IntPtr ca_address, UInt32 offset,
      Byte* data_buf, UInt32 data_length)
{
   Cache* L1_cache = getL1Cache(mem_component);
   switch (mem_op_type)
   {
   case Core::READ:
   case Core::READ_EX:
      L1_cache->accessCacheLine(ca_address + offset, Cache::LOAD, data_buf, data_length);
      break;

   case Core::WRITE:
      L1_cache->accessCacheLine(ca_address + offset, Cache::STORE, data_buf, data_length);
      // Write-through cache - Write the L2 Cache also
      _L2_cache_cntlr->acquireLock();
      _L2_cache_cntlr->assertCacheLineWritable(ca_address);
      _L2_cache_cntlr->writeCacheLine(ca_address, offset, data_buf, data_length);
      _L2_cache_cntlr->releaseLock();
      break;

   default:
      LOG_PRINT_ERROR("Unsupported Mem Op Type: %u", mem_op_type);
      break;
   }
}

bool
L1CacheCntlr::operationPermissibleinL1Cache(MemComponent::Type mem_component, 
      IntPtr address, Core::mem_op_t mem_op_type,
      UInt32 access_num)
{
   bool cache_hit = false;
   CacheState::Type cstate = getCacheLineState(mem_component, address);
   
   switch (mem_op_type)
   {
   case Core::READ:
      cache_hit = CacheState(cstate).readable();
      break;

   case Core::READ_EX:
   case Core::WRITE:
      cache_hit = CacheState(cstate).writable();
      break;

   default:
      LOG_PRINT_ERROR("Unsupported mem_op_type: %u", mem_op_type);
      break;
   }

   if (access_num == 1)
   {
      // Update the Cache Counters
      getL1Cache(mem_component)->updateMissCounters(address, mem_op_type, !cache_hit);
   }

   return cache_hit;
}

void
L1CacheCntlr::insertCacheLine(MemComponent::component_t mem_component,
                              IntPtr address, CacheState::CState cstate, Byte* fill_buf,
                              bool* eviction, PrL1CacheLineInfo* evicted_cache_line_info, IntPtr* evicted_address)
{
   Cache* L1_cache = getL1Cache(mem_component);
   assert(L1_cache);

   PrL1CacheLineInfo L1_cache_line_info;
   L1_cache_line_info.setTag(L1_cache->getTag(address));
   L1_cache_line_info.setCState(cstate);

   L1_cache->insertCacheLine(address, &L1_cache_line_info, fill_buf,
                             eviction, evicted_address, evicted_cache_line_info, NULL);
}

CacheState::Type
L1CacheCntlr::getCacheLineState(MemComponent::Type mem_component, IntPtr address)
{
   Cache* L1_cache = getL1Cache(mem_component);
   assert(L1_cache);

   PrL1CacheLineInfo L1_cache_line_info;
   // Get cache line state
   L1_cache->getCacheLineInfo(address, &L1_cache_line_info);
   return L1_cache_line_info.getCState();
}

void
L1CacheCntlr::setCacheLineState(MemComponent::Type mem_component, IntPtr address, CacheState::Type cstate)
{
   Cache* L1_cache = getL1Cache(mem_component);
   assert(L1_cache);

   PrL1CacheLineInfo L1_cache_line_info;
   L1_cache->getCacheLineInfo(address, &L1_cache_line_info);
   assert(L1_cache_line_info.getCState() != CacheState::INVALID);

   // Set cache line state
   L1_cache_line_info.setCState(cstate);
   L1_cache->setCacheLineInfo(address, &L1_cache_line_info);
}

void
L1CacheCntlr::invalidateCacheLine(MemComponent::Type mem_component, IntPtr address, CacheLineUtilization& cache_line_utilization, UInt64 curr_time)
{
   Cache* L1_cache = getL1Cache(mem_component);
   assert(L1_cache);

   // fprintf(stderr, "Tile(%i): L1(%u): Invalidate(%#lx) - Time(%llu)\n", getTileId(), mem_component, address, (long long unsigned int) curr_time);

   // Invalidate cache line
   L1_cache->invalidateCacheLine(address, cache_line_utilization, curr_time);
}

CacheLineUtilization
L1CacheCntlr::getCacheLineUtilization(MemComponent::Type mem_component, IntPtr address)
{
   Cache* L1_cache = getL1Cache(mem_component);
   assert(L1_cache);

   PrL1CacheLineInfo L1_cache_line_info;
   L1_cache->getCacheLineInfo(address, &L1_cache_line_info);
   return L1_cache_line_info.getUtilization();
}

ShmemMsg::msg_t
L1CacheCntlr::getShmemMsgType(Core::mem_op_t mem_op_type)
{
   switch(mem_op_type)
   {
   case Core::READ:
      return ShmemMsg::SH_REQ;

   case Core::READ_EX:
   case Core::WRITE:
      return ShmemMsg::EX_REQ;

   default:
      LOG_PRINT_ERROR("Unsupported Mem Op Type(%u)", mem_op_type);
      return ShmemMsg::INVALID_MSG_TYPE;
   }
}

Cache*
L1CacheCntlr::getL1Cache(MemComponent::Type mem_component)
{
   switch(mem_component)
   {
   case MemComponent::L1_ICACHE:
      return _L1_icache;

   case MemComponent::L1_DCACHE:
      return _L1_dcache;

   default:
      LOG_PRINT_ERROR("Unrecognized Memory Component(%u)", mem_component);
      return NULL;
   }
}

void
L1CacheCntlr::acquireLock(MemComponent::Type mem_component)
{
   switch(mem_component)
   {
   case MemComponent::L1_ICACHE:
      _L1_icache_lock.acquire();
      break;
   
   case MemComponent::L1_DCACHE:
      _L1_dcache_lock.acquire();
      break;
   
   default:
      LOG_PRINT_ERROR("Unrecognized mem_component(%u)", mem_component);
      break;
   }

}

void
L1CacheCntlr::releaseLock(MemComponent::Type mem_component)
{
   switch(mem_component)
   {
   case MemComponent::L1_ICACHE:
      _L1_icache_lock.release();
      break;
   
   case MemComponent::L1_DCACHE:
      _L1_dcache_lock.release();
      break;
   
   default:
      LOG_PRINT_ERROR("Unrecognized mem_component(%u)", mem_component);
      break;
   }
}

void
L1CacheCntlr::waitForNetworkThread()
{
   _user_thread_sem->wait();
}

void
L1CacheCntlr::wakeUpNetworkThread()
{
   _network_thread_sem->signal();
}

tile_id_t
L1CacheCntlr::getTileId()
{
   return _memory_manager->getTile()->getId();
}

UInt32
L1CacheCntlr::getCacheLineSize()
{ 
   return _memory_manager->getCacheLineSize();
}
 
ShmemPerfModel*
L1CacheCntlr::getShmemPerfModel()
{ 
   return _memory_manager->getShmemPerfModel();
}

}
