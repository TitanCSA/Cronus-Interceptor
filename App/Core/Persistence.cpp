#include "Persistence.h"
#include <Windows.h>
#include <bcrypt.h>
#include <unordered_map>
#include <stdexcept>
#include <chrono>
namespace gpr {
namespace {struct Record{unsigned schema=1;int values[64]{};unsigned char present[64]{};};}
PersistentMemory::PersistentMemory(const std::string& identity){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char digest[32]{};
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA-256 provider unavailable.");
 NTSTATUS code=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0);if(code>=0)code=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(identity.data())),static_cast<ULONG>(identity.size()),0);if(code>=0)code=BCryptFinishHash(hash,digest,32,0);if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);if(code<0)throw std::runtime_error("Could not identify persistent GPC memory.");
 key_=L"Software\\TitanCSA\\CronusInterceptor\\Memory\\";const wchar_t* hex=L"0123456789abcdef";for(auto b:digest){key_+=hex[b>>4];key_+=hex[b&15];}
 Record record;DWORD bytes=sizeof(record);auto status=RegGetValueW(HKEY_CURRENT_USER,key_.c_str(),L"Data",RRF_RT_REG_BINARY,nullptr,&record,&bytes);if(status==ERROR_SUCCESS&&bytes==sizeof(record)&&record.schema==1){for(unsigned i=0;i<64;++i){values_[i]=record.values[i];present_[i]=record.present[i]!=0;}}else if(status!=ERROR_FILE_NOT_FOUND)error_=status==ERROR_SUCCESS?ERROR_INVALID_DATA:status;
 worker_=std::thread([this]{Run();});
}
PersistentMemory::~PersistentMemory(){{std::lock_guard<std::mutex> lock(mutex_);stop_=true;}changed_.notify_one();if(worker_.joinable())worker_.join();}
std::shared_ptr<PersistentMemory> PersistentMemory::Open(const std::string& key){static std::mutex gate;static std::unordered_map<std::string,std::weak_ptr<PersistentMemory>> stores;std::lock_guard<std::mutex> lock(gate);if(auto memory=stores[key].lock())return memory;auto memory=std::shared_ptr<PersistentMemory>(new PersistentMemory(key));stores[key]=memory;return memory;}
int PersistentMemory::Read(unsigned i,int fallback)const noexcept{return i<64&&present_[i].load(std::memory_order_acquire)?values_[i].load(std::memory_order_relaxed):fallback;}
void PersistentMemory::Write(unsigned i,int value)noexcept{if(i>=64)return;if(present_[i].load()&&values_[i].load()==value)return;values_[i].store(value);present_[i].store(true,std::memory_order_release);dirty_.store(true);changed_.notify_one();}
void PersistentMemory::Flush()noexcept{if(!dirty_.exchange(false))return;Record record;for(unsigned i=0;i<64;++i){record.values[i]=values_[i].load();record.present[i]=present_[i].load()?1:0;}HKEY key=nullptr;auto status=RegCreateKeyExW(HKEY_CURRENT_USER,key_.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(status==ERROR_SUCCESS){status=RegSetValueExW(key,L"Data",0,REG_BINARY,reinterpret_cast<const BYTE*>(&record),sizeof(record));RegCloseKey(key);}error_=status;}
void PersistentMemory::Run()noexcept{try{std::unique_lock<std::mutex> lock(mutex_);while(!stop_){changed_.wait(lock,[&]{return stop_||dirty_.load();});if(!stop_)changed_.wait_for(lock,std::chrono::milliseconds(500),[&]{return stop_;});lock.unlock();Flush();lock.lock();}lock.unlock();Flush();}catch(...){error_=ERROR_GEN_FAILURE;}}
}
