#ifndef __MIPS_Simulator__Memsys__
#define __MIPS_Simulator__Memsys__

#include "cache.h"
#include "memory.h"
#include <string>

class MemSys {
 public:
  MemSys(Cache *cache, Memory *mainMemory, bool cacheOn);
  int loadWord(uint32_t add, uint32_t *val);
  int storeWord(uint32_t add, uint32_t val);
  int loadByte(uint32_t add, uint8_t *val);
  int storeByte(uint32_t add, uint8_t val);
  int directStoreByte(uint32_t add, uint8_t val);
  void dump(std::string fn);
  bool _cacheOn;
  void resetCache() { _cache->reset(); }
  uint32_t _memSize;
  int _cacheSize;
  int _lineSize;
  Cache *_cache;
  Memory *_mainMemory;
};

#endif /* defined(__MIPS_Simulator__Memsys__) */
