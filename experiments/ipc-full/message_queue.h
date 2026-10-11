// SPDX-License-Identifier: MIT
#pragma once
#include "upload_pool.h"
#include "util_commands.h"
#include <optional>
#include <map>
#include <deque>
#include <mutex>
#include <array>
extern std::atomic<bool> gbBridgeRunning;
namespace ldb_full {
struct Session {
  std::mutex mutex; std::array<ldb_ipc_v2::Channel*,32> channels{};
  void attach(ldb_ipc_v2::Channel* c){std::lock_guard<std::mutex> l(mutex);for(auto& s:channels)if(!s){s=c;return;}throw std::runtime_error("session channel capacity");}
  void detach(ldb_ipc_v2::Channel* c){std::lock_guard<std::mutex> l(mutex);for(auto& s:channels)if(s==c){s=nullptr;return;}}
  void fault(){gbBridgeRunning.store(false,std::memory_order_release);std::lock_guard<std::mutex> l(mutex);for(auto* c:channels)if(c)c->fault(ldb_ipc_v2::Fault::Handler);}
};
inline Session& session(){static Session s;return s;}

constexpr uint16_t Inline=0x4000, External=0x2000;
constexpr uint32_t InlineBytes=256;
class MessageQueue {
  std::string m_name;
  bool m_writerSide;
  mutable ldb_ipc_v2::Channel m_channel;
  UploadPool m_pool;
  std::unique_ptr<ldb_ipc_v2::Writer> m_writer;
  std::optional<ldb_ipc_v2::Encoder> m_encoder;
  Header m_writeHeader{},m_header{};
  Descriptor m_writeLease{},m_readLease{};
  bool m_external=false,m_borrowed=false,m_blob=false,m_hasDeclaration=false,m_current=false,m_readExternal=false;
  uint32_t m_declared=0,m_written=0,m_pending=0,m_batch=1;
  std::atomic<uint64_t> m_deadline{0};
  uint32_t m_sequence=0,m_offset=0,m_recordsRead=0,m_recordBytes=0;
  const uint8_t* m_read=nullptr;
  uint32_t m_readBytes=0,m_readCursor=0;
  uint64_t m_sentBytes=0,m_receivedBytes=0;
  std::map<uint32_t,std::deque<Descriptor>> m_pins;
  mutable std::mutex m_peekMutex;
  static ldb_ipc_v2::Config config(const std::string& name) {
    ldb_ipc_v2::Config c;c.blockCount=64;c.blockBytes=4096;
    c.kind=name.find("Module")==std::string::npos?1:2;
    c.direction=name.find("Server2Client")==std::string::npos?0:1;return c;
  }
  static uint32_t budget(size_t bytes) {
    if(bytes<PoolMetadata+PageBytes || bytes>256u*1024*1024)throw std::invalid_argument("upload pool budget");
    return static_cast<uint32_t>(bytes);
  }
  bool ensure(uint32_t additional,bool reserved=false) {
    if(m_blob || additional>m_pool.capacity()-m_written || !healthy()
       || (m_hasDeclaration && additional>m_declared-m_written))return false;
    const uint32_t end=m_written+additional;
    if(!m_external && !reserved && end<=InlineBytes)return true;
    if(!m_external) {
      std::array<uint8_t,InlineBytes> prefix{};
      if(m_written)std::memcpy(prefix.data(),m_encoder->bodyData(),m_written);
      m_encoder.reset(); // Abort only the unpublished current record.
      const uint32_t wanted=std::max(end,m_declared);
      bool allocated=m_pool.allocate(wanted,m_writeLease);
      if(!allocated) {
        if(!flush())return false;
        ldb_ipc_v2::Metrics metrics;
        const auto r=m_channel.wait(true,[&](){return allocated || (allocated=m_pool.allocate(wanted,m_writeLease)) || !healthy();},2000,128,
          m_channel.shared().cold.consumerPid.load(),metrics);
        if(r!=ldb_ipc_v2::Result::Success || !allocated || !healthy())return false;
      }
      m_external=true;m_writeLease.bytes=m_written;
      if(m_written)std::memcpy(m_pool.pointer(m_writeLease),prefix.data(),m_written);
    }
    if(end && !m_pool.grow(m_writeLease,std::max(end,m_declared),m_borrowed)) {
      if(!flush())return false;
      bool grown=false;ldb_ipc_v2::Metrics metrics;
      const auto r=m_channel.wait(true,[&](){return grown || (grown=m_pool.grow(m_writeLease,std::max(end,m_declared),m_borrowed)) || !healthy();},2000,128,
        m_channel.shared().cold.consumerPid.load(),metrics);
      if(r!=ldb_ipc_v2::Result::Success || !grown || !healthy())return false;
    }
    return true;
  }
  uint8_t* writePointer(){return m_external?m_pool.pointer(m_writeLease):m_encoder->bodyData();}
  void wrote(uint32_t bytes){m_written+=bytes;if(m_external)m_writeLease.bytes=m_written;}
  bool notifyRelease(const Descriptor& d){return !m_pool.release(d) || m_channel.notifyCredits();}
public:
  static size_t getExtraMemoryRequirements(){return 0;}
  MessageQueue(const std::string& name,size_t bytes,bool writer)
    :m_name(name),m_writerSide(writer),m_channel(name+"-full",config(name),false,true),
     m_pool(name+"-uploads",budget(bytes),writer,config(name)) {
    if(writer)m_writer=std::make_unique<ldb_ipc_v2::Writer>(m_channel,2000,128);
    else {
      uint32_t empty=0;
      if(!m_channel.shared().cold.consumerPid.compare_exchange_strong(empty,ldb_ipc_v2::processId()))throw std::runtime_error("duplicate full reader");
      m_sequence=m_channel.shared().consumer.completed.load();
    }
    session().attach(&m_channel);
  }
  ~MessageQueue(){session().detach(&m_channel);if(!m_writerSide){m_channel.shared().cold.consumerClosed.store(1,std::memory_order_seq_cst);m_channel.notifyCredits();}}
  MessageQueue(const MessageQueue&)=delete;
  bool healthy()const{return m_channel.healthy();}
  void fault(){session().fault();}
  uint32_t capacity()const{return m_pool.capacity();}
  uint64_t cursor()const{return m_writerSide?m_sentBytes+m_written:m_receivedBytes+m_readCursor;}
  const std::string& name()const{return m_name;}
  bool blobOpen()const{return m_blob;}
  uint32_t bodyBytes()const{return m_written;}
  void configureBatch(uint32_t batch){if(m_encoder || m_external)throw std::logic_error("batch during command");m_batch=batch==32?32:1;}
  uint32_t batchSize()const{return m_batch;}
  bool pending()const{return m_deadline.load(std::memory_order_acquire)!=0;}
  bool due(uint64_t now)const{const auto d=m_deadline.load(std::memory_order_acquire);return d && now>=d;}
  bool flush(){if(m_encoder)return false;if(!m_writerSide)return true;
    if(m_writer->flush()!=ldb_ipc_v2::Result::Success)return false;
    m_pending=0;m_deadline.store(0,std::memory_order_release);return true;}
  bool begin(const Header& h) {
    if(m_encoder || m_external || !healthy())return false;
    m_writeHeader=h;m_written=m_declared=0;m_hasDeclaration=false;m_borrowed=m_blob=false;
    auto e=m_writer->begin(static_cast<uint16_t>(h.command),h.pHandle,0,InlineBytes,static_cast<uint16_t>(h.flags|Inline));
    if(!e.valid())return false;m_encoder.emplace(std::move(e));return true;
  }
  bool declare(uint64_t bytes,bool reserved=false) {
    if(bytes>m_pool.capacity() || bytes<m_written)return false;
    m_hasDeclaration=true;m_declared=static_cast<uint32_t>(bytes);return ensure(m_declared-m_written,reserved);
  }
  bool scalar(uint32_t value) {
    if(!ensure(4))return false;
    if(m_external){std::memcpy(writePointer()+m_written,&value,4);wrote(4);return true;}
    if(!m_encoder->scalar(value))return false;wrote(4);return true;
  }
  uint8_t* beginBlob(size_t bytes,bool reserved=true) {
    if(bytes>UINT32_MAX || bytes>m_pool.capacity())return nullptr;
    const uint64_t wire=4+((static_cast<uint64_t>(bytes)+3)&~uint64_t{3});
    if(wire>UINT32_MAX || !ensure(static_cast<uint32_t>(wire),reserved))return nullptr;
    uint8_t* pointer=nullptr;
    if(m_external) {
      auto* base=writePointer()+m_written;const auto size=static_cast<uint32_t>(bytes);std::memcpy(base,&size,4);
      pointer=base+4;const auto padding=static_cast<uint32_t>(wire)-4-size;
      if(padding)std::memset(pointer+size,0,padding);
    }else pointer=m_encoder->reserveBlob(static_cast<uint32_t>(bytes));
    if(!pointer)return nullptr;wrote(static_cast<uint32_t>(wire));m_blob=true;m_borrowed|=reserved;return pointer;
  }
  bool endBlob(){if(!m_blob)return false;m_blob=false;return true;}
  bool blob(size_t bytes,const void* object) {
    if(!object)return scalar(0);
    auto* p=beginBlob(bytes,false);if(!p)return false;if(bytes)std::memcpy(p,object,bytes);return endBlob();
  }
  void abort() {
    m_encoder.reset();if(m_external){m_pool.abort(m_writeLease);m_external=false;}m_blob=false;
  }
  bool finish(bool force) {
    if(!healthy() || m_blob || (m_hasDeclaration && m_written!=m_declared))return false;
    if(m_external) {
      auto e=m_writer->begin(static_cast<uint16_t>(m_writeHeader.command),m_writeHeader.pHandle,0,sizeof(Descriptor),static_cast<uint16_t>(m_writeHeader.flags|External));
      if(!e.valid())return false;
      const uint32_t fields[4]={m_writeLease.slot,m_writeLease.generation,m_writeLease.offset,m_writeLease.bytes};
      for(auto v:fields)if(!e.scalar(v))return false;
      m_pool.publish(m_writeLease);
      m_external=false;
      if(e.finish(false)!=ldb_ipc_v2::Result::Success)return false;force=true; // leased payloads publish without deferred ownership
    } else {
      if(!m_encoder || m_encoder->finish(false)!=ldb_ipc_v2::Result::Success)return false;m_encoder.reset();
    }
    m_sentBytes+=m_written;m_written=0;m_declared=0;
    if(!m_pending)m_deadline.store(ldb_ipc_v2::milliseconds()+1,std::memory_order_release);
    if(++m_pending>=m_batch || force)return flush();return true;
  }
  Header peek(bridge_util::Result& result,uint32_t timeout=0,std::atomic<bool>* early=nullptr) {
    std::lock_guard<std::mutex> lock(m_peekMutex);
    try {
      if(!m_current) {
        auto& shared=m_channel.shared();uint32_t published=shared.producer.published.load(std::memory_order_acquire);
        if(!m_offset && published==m_sequence) {
          ldb_ipc_v2::Metrics metrics;const auto r=m_channel.wait(false,[&](){published=shared.producer.published.load(std::memory_order_seq_cst);return published!=m_sequence || !healthy() || (early && early->load()) || shared.cold.producerClosed.load(std::memory_order_seq_cst);},timeout?timeout:2000,128,shared.cold.producerPid.load(),metrics);
          if(r==ldb_ipc_v2::Result::Timeout || (early && early->load())){result=bridge_util::Result::Timeout;return {};}
          if(r!=ldb_ipc_v2::Result::Success)throw std::runtime_error("message wait failed");
        }
        if(!healthy() || published==m_sequence)throw std::runtime_error("closed message channel");
        const auto& b=m_channel.block(m_sequence);
        if(uint32_t(published-m_sequence)>shared.config.blockCount || b.sequence!=m_sequence || !b.records || b.bytes>shared.config.blockBytes
           || m_recordsRead>=b.records || m_offset>b.bytes || b.bytes-m_offset<sizeof(ldb_ipc_v2::Record))throw std::runtime_error("invalid message block");
        ldb_ipc_v2::Record record{};const auto* base=m_channel.data(m_channel.block(m_sequence))+m_offset;std::memcpy(&record,base,sizeof(record));
        if(record.bytes<sizeof(record) || record.bytes%4 || record.bytes>b.bytes-m_offset || record.bodyBytes!=record.bytes-sizeof(record)
           || record.generation || record.uid || (record.flags & ~(Inline|External|Commands::DataInSharedHeap|Commands::DataIsReserved)) || ((record.flags&(Inline|External))!=Inline && (record.flags&(Inline|External))!=External))throw std::runtime_error("invalid full record");
        m_readExternal=(record.flags&External)!=0;
        m_header={static_cast<Commands::D3D9Command>(record.command),static_cast<uint16_t>(record.flags&~(Inline|External)),0,record.resource};
        if(m_readExternal) {
          if(record.bodyBytes!=sizeof(Descriptor))throw std::runtime_error("invalid upload record");
          std::memcpy(&m_readLease,base+sizeof(record),sizeof(m_readLease));m_read=m_pool.acquire(m_readLease);m_readBytes=m_readLease.bytes;
        }else {m_read=base+sizeof(record);m_readBytes=record.bodyBytes;}
        if(m_readBytes%4)throw std::runtime_error("unaligned body");m_readCursor=0;m_recordBytes=record.bytes;m_current=true;
      }
      result=bridge_util::Result::Success;return m_header;
    }catch(...){fault();result=bridge_util::Result::Failure;return {};}
  }
  Header pull(bridge_util::Result& r,uint32_t t=0,std::atomic<bool>* e=nullptr){return peek(r,t,e);}
  uint32_t peekWord()const {if(!m_current || m_readCursor>m_readBytes || m_readBytes-m_readCursor<4)throw std::runtime_error("body scalar bounds");uint32_t v;std::memcpy(&v,m_read+m_readCursor,4);return v;}
  const uint32_t& readWord(){peekWord();auto* p=reinterpret_cast<const uint32_t*>(m_read+m_readCursor);m_readCursor+=4;return *p;}
  const uint32_t& readBlob(void** out) {
    const auto bytes=peekWord();const uint64_t wire=4+((uint64_t{bytes}+3)&~uint64_t{3});
    if(wire>m_readBytes-m_readCursor)throw std::runtime_error("body blob bounds");
    auto* len=reinterpret_cast<const uint32_t*>(m_read+m_readCursor);*out=bytes?const_cast<uint8_t*>(m_read+m_readCursor+4):nullptr;m_readCursor+=static_cast<uint32_t>(wire);return *len;
  }
  uint32_t remaining()const{return m_readBytes-m_readCursor;}
  void pin(uint32_t id){if(!m_current || !m_readExternal)throw std::runtime_error("pin requires upload lease");m_pool.retain(m_readLease);m_pins[id].push_back(m_readLease);}
  void unpin(uint32_t id){auto i=m_pins.find(id);if(i==m_pins.end())return;if(!i->second.empty()){if(!notifyRelease(i->second.front()))throw std::runtime_error("lease credit failed");i->second.pop_front();}if(i->second.empty())m_pins.erase(i);}
  void destroyPins(uint32_t id){while(m_pins.count(id))unpin(id);}
  void finishRead() {
    std::lock_guard<std::mutex> lock(m_peekMutex);
    if(!m_current || m_readCursor!=m_readBytes)throw std::runtime_error("incomplete message body");
    if(m_readExternal && !notifyRelease(m_readLease))throw std::runtime_error("upload release failed");
    m_receivedBytes+=m_readBytes;m_offset+=m_recordBytes;++m_recordsRead;m_current=false;
    const auto& b=m_channel.block(m_sequence);
    if(m_recordsRead==b.records) {
      if(m_offset!=b.bytes)throw std::runtime_error("message tail mismatch");m_offset=m_recordsRead=0;
      m_channel.shared().consumer.completed.store(++m_sequence,std::memory_order_seq_cst);
      if(!m_channel.notifyCredits())throw std::runtime_error("message credit failed");
    }else if(m_offset>=b.bytes)throw std::runtime_error("message record count");
  }
  bool isEmpty()const{return !m_current && m_channel.shared().producer.published.load(std::memory_order_acquire)==m_sequence;}
  std::vector<Commands::D3D9Command> getWriterQueueData(int=10){return {};}
  std::vector<Commands::D3D9Command> getReaderQueueData(int=10){return {};}
};
}
