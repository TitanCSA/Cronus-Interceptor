#include "../Core/Script.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace gpr;
unsigned checks=0;
void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
ControlFrame Frame(){ControlFrame f;f.Family=2;f.Available.fill(true);return f;}
void Test(const char* code,int slot,int value){auto p=ScriptCompiler::Compile(code);auto vm=p->CreateInstance();auto f=Frame();vm->Execute(f,1000000,10);Check((slot>=208&&slot<=213?vm->Debug().Traces[slot-208]:f.Values[slot])==value,code);}
int main(int argc,char** argv){try{
 if(argc>2&&std::string(argv[1])=="--public-behavior"){
  auto load=[&](const std::string& name,int bits){std::filesystem::path found;for(const auto& e:std::filesystem::recursive_directory_iterator(argv[2]))if(e.path().filename().string()==name){if(!found.empty())throw std::runtime_error("Duplicate fixture source");found=e.path();}if(found.empty())throw std::runtime_error("Public fixture missing");std::ifstream file(found);std::stringstream code;code<<file.rdbuf();return ScriptCompiler::Compile(code.str(),{bits})->CreateInstance();};
  for(int bits:{16,32}){
   {auto vm=load("MGS pressure sensitive button limiter.gpc",bits);auto f=Frame();f.Physical[12]=80;vm->Execute(f,0,0);Check(f.Values[12]==80,"Public pressure limiter starts disabled");f.Physical[16]=100;vm->Execute(f,10000,10);Check(f.Values[12]==50,"Public pressure limiter clamps when toggled");f.Physical[16]=0;vm->Execute(f,20000,10);f.Physical[16]=100;vm->Execute(f,30000,10);Check(f.Values[12]==80,"Public pressure limiter second toggle restores");}
   {auto vm=load("modulo 2 test.gpc",bits);auto f=Frame();vm->Execute(f,0,0);Check(vm->Debug().Traces[0]==1&&vm->Debug().Traces[1]==3,"Public modulo initial traces");f.Physical[6]=100;vm->Execute(f,10000,10);Check(vm->Debug().Traces[0]==0&&vm->Debug().Traces[1]==4&&vm->Debug().Leds[0]==0,"Public modulo edge and virtual LED");vm->Execute(f,20000,10);Check(vm->Debug().Traces[1]==4,"Public modulo does not repeat held edge");}
   {auto vm=load("moonlight ps.gpc",bits);auto f=Frame();f.Physical[17]=100;f.Physical[1]=75;vm->Execute(f,0,0);Check(f.Values[17]==0&&f.Values[1]==0&&f.Values[20]==100,"Public Moonlight chord to guide");}
   {auto vm=load("Inverted Throttle.gpc",bits);auto f=Frame();for(int value=0;value<=100;++value){f.Physical[5]=value;vm->Execute(f,static_cast<long long>(value)*10000,10);Check(f.Values[5]==100-value,"Public inverted throttle complete range");}}
  }
  std::cout<<"PASS "<<checks<<" expected public-script behavior assertions in 16/32-bit modes; no physical Cronus oracle.\n";return 0;
 }
 if(argc>2&&std::string(argv[1])=="--corpus"){
  unsigned accepted=0,rejected=0,executed=0,faults=0;
  for(const auto& e:std::filesystem::recursive_directory_iterator(argv[2]))if(e.path().extension()==L".gpc"){
   std::ifstream file(e.path(),std::ios::binary);std::stringstream text;text<<file.rdbuf();
   try{auto p=ScriptCompiler::Compile(text.str());++accepted;auto v=p->CreateInstance();auto f=Frame();try{for(int t=0;t<1000;t+=10){f.Physical.fill(0);f.Physical[10]=(t%200)<100?100:0;f.Physical[4]=t%101;f.Physical[0]=t%201-100;v->Execute(f,static_cast<long long>(t)*1000,10);}++executed;std::cout<<"RUN\t"<<e.path().filename().string()<<"\n";}catch(const std::exception& err){++faults;std::cout<<"FAULT\t"<<e.path().filename().string()<<"\t"<<err.what()<<"\n";}}
   catch(const ScriptException& err){++rejected;std::cout<<"REJECT\t"<<e.path().filename().string()<<"\t"<<err.Diagnostic()<<"\n";}
  }
  std::cout<<"CORPUS compiled="<<accepted<<" rejected="<<rejected<<" executed="<<executed<<" runtime_faults="<<faults<<"\n";return 0;
 }
 Test("main {set_val(XB1_A,100);}",10,100);
 Test("main {set_val(19,100);}",10,100);
 Test("int a;main {a=7;set_bit(a,3);set_val(TRACE_1,a);}",208,15);
 Test("data(0x12,0x34,255); main {set_val(TRACE_1,dword(0));}",208,0x1234);
 Test("int n=5;function f(a){return a*2;}main{set_val(TRACE_1,f(n));}",208,10);
 Test("remap XB1_A -> XB1_B;main{set_val(XB1_A,100);}",11,100);
 auto source=ScriptCompiler::Compile("int x=32767;main{x++;set_val(TRACE_1,x);}",{16});auto vm=source->CreateInstance();auto f=Frame();vm->Execute(f,0,0);Check(vm->Debug().Traces[0]==-32768,"16-bit integer wrap");
 if(argc>1){for(const auto& e:std::filesystem::directory_iterator(argv[1]))if(e.path().extension()==L".gpc"){std::ifstream file(e.path());std::stringstream text;text<<file.rdbuf();(void)ScriptCompiler::Compile(text.str());++checks;}}
 std::cout<<"PASS "<<checks<<" checks\n";return 0;
 }catch(const ScriptException& e){std::cerr<<e.Diagnostic()<<"\n";return 1;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
