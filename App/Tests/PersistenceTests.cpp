#include "../Core/Persistence.h"
#include <Windows.h>
#include <bcrypt.h>
#include <iostream>
#include <stdexcept>
void Check(bool v){if(!v)throw std::runtime_error("Isolated persistence assertion failed");}
int main(){std::wstring key;try{
 const auto identity="cronus-test:"+std::to_string(GetCurrentProcessId())+":"+std::to_string(GetTickCount64());
 BCRYPT_ALG_HANDLE alg=nullptr;unsigned char digest[32]{};Check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0);auto code=BCryptHash(alg,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(identity.data())),static_cast<ULONG>(identity.size()),digest,32);BCryptCloseAlgorithmProvider(alg,0);Check(code>=0);
 key=L"Software\\TitanCSA\\CronusInterceptor\\Memory\\";const wchar_t* hex=L"0123456789abcdef";for(auto b:digest){key+=hex[b>>4];key+=hex[b&15];}
 HKEY existing=nullptr;Check(RegOpenKeyExW(HKEY_CURRENT_USER,key.c_str(),0,KEY_READ,&existing)==ERROR_FILE_NOT_FOUND);
 {auto memory=gpr::PersistentMemory::Open(identity);Check(memory->Read(0,99)==99);memory->Write(0,37);Check(memory->Read(0,99)==37);auto shared=gpr::PersistentMemory::Open(identity);Check(shared==memory);}
 {auto memory=gpr::PersistentMemory::Open(identity);Check(memory->Read(0,99)==37);Check(memory->Error()==0);}
 HKEY registry=nullptr;Check(RegOpenKeyExW(HKEY_CURRENT_USER,key.c_str(),0,KEY_SET_VALUE,&registry)==0);DWORD malformed=5;auto result=RegSetValueExW(registry,L"Data",0,REG_DWORD,reinterpret_cast<BYTE*>(&malformed),sizeof(malformed));RegCloseKey(registry);Check(result==0);
 {auto memory=gpr::PersistentMemory::Open(identity);Check(memory->Read(0,99)==99);Check(memory->Error()!=0);}
 Check(RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str())==0);std::cout<<"PASS isolated registry roundtrip, shared cache, async final flush, corrupt-type fallback and cleanup.\n";return 0;
 }catch(const std::exception& e){if(!key.empty())RegDeleteTreeW(HKEY_CURRENT_USER,key.c_str());std::cerr<<e.what()<<"\n";return 1;}}
