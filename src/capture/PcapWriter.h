#pragma once
#include "../storage/StorageManager.h"

namespace dirk {
class PcapWriter {
public:
  bool begin(StorageManager& s,const char* path,uint32_t linktype=105){
    storage=&s; strlcpy(file,path,sizeof(file));
    uint32_t hdr[6]={0xa1b2c3d4,0x00040002,0,0,65535,linktype};
    return storage->write(file,(uint8_t*)hdr,sizeof(hdr));
  }
  bool packet(const uint8_t* data,uint32_t n,uint32_t sec,uint32_t usec){
    if(!storage || !storage->mounted()) return false;
    File f=SD.open(file,FILE_APPEND); if(!f) return false;
    uint32_t ph[4]={sec,usec,n,n};
    bool ok=f.write((uint8_t*)ph,sizeof(ph))==sizeof(ph) && f.write(data,n)==n;
    f.close(); return ok;
  }
private:
  StorageManager* storage=nullptr;
  char file[96]{};
};
}
