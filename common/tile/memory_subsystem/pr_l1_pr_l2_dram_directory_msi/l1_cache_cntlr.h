#pragma once

#include <string>
using std::string;

// Forward declaration
namespace PrL1PrL2DramDirectoryMSI
{
   class L2CacheCntlr;
   class MemoryManager;
}

#include "tile.h"
#include "cache.h"
#include "pr_l1_cache_line_info.h"
#include "shmem_msg.h"
#include "mem_component.h"
#include "semaphore.h"
#include "lock.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class L1CacheCntlr
   {
   public:
      L1CacheCntlr(MemoryManager* memory_manager,
                   Semaphore* user_thread_sem,
                   Semaphore* network_thread_sem,
                   UInt32 cache_line_size,
                   UInt32 l1_icache_size,
                   UInt32 l1_icache_associativity,
                   string l1_icache_replacement_policy,
                   UInt32 l1_icache_access_delay,
                   bool l1_icache_track_miss_types,
                   UInt32 l1_dcache_size,
                   UInt32 l1_dcache_associativity,
                   string l1_dcache_replacement_policy,
                   UInt32 l1_dcache_access_delay,
                   bool l1_dcache_track_miss_types,
                   volatile float frequency);
      ~L1CacheCntlr();

      Cache* getL1ICache() { return _l1_icache; }
      Cache* getL1DCache() { return _l1_dcache; }

      void setL2CacheCntlr(L2CacheCntlr* l2_cache_cntlr);

      bool processMemOpFromTile(MemComponent::Type mem_component,
            Core::lock_signal_t lock_signal,
            Core::mem_op_t mem_op_type, 
            IntPtr ca_address, UInt32 offset,
            Byte* data_buf, UInt32 data_length,
            bool modeled);

      void insertCacheLine(MemComponent::Type mem_component,
            IntPtr address, CacheState::Type cstate, Byte* fill_buf,
            bool* eviction, IntPtr* evicted_address);

      CacheState::Type getCacheLineState(MemComponent::Type mem_component, IntPtr address);
      void setCacheLineState(MemComponent::Type mem_component, IntPtr address, CacheState::Type cstate);
      void invalidateCacheLine(MemComponent::Type mem_component, IntPtr address);

      void acquireLock(MemComponent::Type mem_component);
      void releaseLock(MemComponent::Type mem_component);
   
   private:
      MemoryManager* _memory_manager;
      Cache* _l1_icache;
      Cache* _l1_dcache;
      L2CacheCntlr* _l2_cache_cntlr;

      Lock _l1_icache_lock;
      Lock _l1_dcache_lock;
      Semaphore* _user_thread_sem;
      Semaphore* _network_thread_sem;

      void accessCache(MemComponent::Type mem_component,
            Core::mem_op_t mem_op_type, 
            IntPtr ca_address, UInt32 offset,
            Byte* data_buf, UInt32 data_length);
      bool operationPermissibleinL1Cache(MemComponent::Type mem_component,
            IntPtr address, Core::mem_op_t mem_op_type,
            UInt32 access_num);

      Cache* getL1Cache(MemComponent::Type mem_component);
      ShmemMsg::Type getShmemMsgType(Core::mem_op_t mem_op_type);

      // Utilities
      tile_id_t getTileId();
      UInt32 getCacheLineSize();
      MemoryManager* getMemoryManager()   { return _memory_manager; }
      ShmemPerfModel* getShmemPerfModel();

      // Wait for Network Thread
      void waitForNetworkThread();
      // Wake up Network Thread
      void wakeUpNetworkThread();
   };
}
