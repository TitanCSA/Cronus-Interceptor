#include "../Core/Script.h"
#include "../Capture/ControlCodec.h"
#include <iostream>
#include <limits>
#include <random>
#include <chrono>
#include <functional>
using namespace gpr;
unsigned checks=0;
void Check(bool v,const char* reason){++checks;if(!v)throw std::runtime_error(reason);}
ControlFrame Frame(){ControlFrame f;f.Available.fill(true);f.Family=2;return f;}
int Eval(const std::string& source,int bits=32){auto v=ScriptCompiler::Compile(source,{bits})->CreateInstance();auto f=Frame();v->Execute(f,0,0);return v->Debug().Traces[0];}
void Reject(const std::string& source){try{(void)ScriptCompiler::Compile(source);throw std::runtime_error("Invalid script compiled");}catch(const ScriptException&){++checks;}}
void Fault(const std::string& source){auto v=ScriptCompiler::Compile(source)->CreateInstance();auto f=Frame();try{v->Execute(f,0,0);throw std::runtime_error("Unsafe script executed");}catch(const ScriptException&){++checks;}}
int main(){try{
 Check(Eval("const uint8 a[]={128,255};main{set_val(TRACE_1,a[1]);}")==255,"uint8 literal array");
 Check(Eval("const int8 a[]={128,255};main{set_val(TRACE_1,a[1]);}")==-1,"int8 signed array");
 Check(Eval("const uint16 a[][]={{44444,55555},{60000,65535}};main{set_val(TRACE_1,a[1][1]);}")==65535,"two-dimensional unsigned array");
 Check(Eval("int a=1;function f(a){return a+1;}main{set_val(TRACE_1,f(5));}")==6,"local parameter shadows global");
 Check(Eval("int a=5;main{set_bits(a,3,4,15);clear_bit(a,0);set_val(TRACE_1,get_bits(a,4,15));}")==3,"bit mutation and masks");
 Check(Eval("main{set_val(TRACE_1,1 ^^ 4);}")==0,"logical xor truth values");
 Check(Eval("main{set_val(TRACE_1,0 && (5/0));}")==0,"short-circuit AND");
 Check(Eval("main{set_val(TRACE_1,1 || (5/0));}")==1,"short-circuit OR");
 Check(Eval("enum{A=4,B,C} main{set_val(TRACE_1,C);}")==6,"enum initializers");
 Check(Eval("int a;main{do{a++;}while(a<5);set_val(TRACE_1,a);}")==5,"do while loop");
 Check(Eval("int a=2,n;main{switch(a){case 1:{n=10;break;}case 2:{n=20;}case 3:{n+=3;break;}default:{n=99;}}set_val(TRACE_1,n);}")==23,"switch matching, fallthrough and break");
 Check(Eval("define a=1,b=2;main{set_val(TRACE_1,a+b);}")==3,"definition lists");
 {auto v=ScriptCompiler::Compile("remap XB1_A -> XB1_B;remap XB1_B -> XB1_A;main{}")->CreateInstance();auto f=Frame();f.Physical[10]=100;v->Execute(f,0,0);Check(f.Values[10]==0&&f.Values[11]==100,"parallel static remap pair");}
 for(int bit:{16,32})for(int x=-100;x<=100;++x){auto f=Frame();f.Physical[0]=x;auto v=ScriptCompiler::Compile("main{set_val(XB1_LX,inv(get_ival(XB1_LX)));}",{bit})->CreateInstance();v->Execute(f,0,0);Check(f.Values[0]==-x,"signed stick inverse");}
 Check(Eval("int n=2147483647;main{n++;set_val(TRACE_1,n);}")==INT_MIN,"32-bit defined wrap");
 Check(Eval("int n=32767;main{n++;set_val(TRACE_1,n);}",16)==-32768,"16-bit defined wrap");
 Check(Eval("data(0x80,0x12,0x34);main{set_val(TRACE_1,dint8(0));}")==-128,"signed data byte");
 {auto v=ScriptCompiler::Compile("main{set_val(XB1_A,0);set_val(XB1_B,get_val(XB1_A));set_val(TRACE_1,get_ival(XB1_A));}")->CreateInstance();auto f=Frame();f.Physical[10]=100;v->Execute(f,0,0);Check(f.Values[10]==0&&f.Values[11]==0&&v->Debug().Traces[0]==100,"mutable output vs physical input");}
 {auto v=ScriptCompiler::Compile("int n;main{if(event_press(XB1_A))n++;set_val(TRACE_1,n);set_val(TRACE_2,get_lval(XB1_A));set_val(TRACE_3,get_ptime(XB1_A));}")->CreateInstance();auto f=Frame();v->Execute(f,0,0);f.Physical[10]=100;v->Execute(f,10000,10);Check(v->Debug().Traces[0]==1&&v->Debug().Traces[1]==0,"one-cycle press event");v->Execute(f,20000,10);Check(v->Debug().Traces[0]==1&&v->Debug().Traces[1]==100&&v->Debug().Traces[2]==10,"hold timer and previous physical value");}
 {auto v=ScriptCompiler::Compile("int n;main{n++;vm_tctrl(-5);set_val(TRACE_1,n);set_val(TRACE_2,get_rtime());}")->CreateInstance();auto f=Frame();v->Execute(f,0,0);Check(v->NextWakeMilliseconds()==5,"vm offset uses 10 ms base");v->Execute(f,1000,1);Check(v->Debug().Traces[0]==1&&v->NextWakeMilliseconds()==4,"physical reports do not speed up VM");v->Execute(f,5000,4);Check(v->Debug().Traces[0]==2&&v->Debug().Traces[1]==5,"VM elapsed interval");}
 {auto v=ScriptCompiler::Compile("main{if(event_press(XB1_X))combo_run(P);}combo P{call(C);set_val(XB1_B,100);wait(20);}combo C{set_val(XB1_A,100);wait(20);wait(20);}")->CreateInstance();auto f=Frame();f.Physical[12]=100;v->Execute(f,0,0);Check(f.Values[10]==100&&f.Values[11]==0,"child combo first step");f.Physical[12]=0;v->Execute(f,10000,10);Check(f.Values[10]==100,"child holds across cycles");v->Execute(f,20000,10);Check(f.Values[10]==0,"wait-only segment releases previous controls");v->Execute(f,40000,20);Check(f.Values[11]==100,"parent resumes after child");v->Execute(f,60000,20);v->Execute(f,70000,10);Check(f.Values[11]==0,"completed combo passthrough");}
 {auto v=ScriptCompiler::Compile("main{block(XB1_A,20);}")->CreateInstance();auto f=Frame();f.Physical[10]=100;v->Execute(f,0,0);Check(f.Values[10]==0,"initial timed block");v->Execute(f,10000,10);v->Execute(f,20000,10);Check(f.Values[10]==100,"block relative to state edge, not endless rearm");}
 for(int raw=-32768;raw<=32767;++raw){IC_NATIVE_GAMEPAD_STATE s{};s.left_x=static_cast<short>(raw);s.left_y=static_cast<short>(raw);s.right_x=static_cast<short>(raw);s.right_y=static_cast<short>(raw);std::array<int,256> values{};std::array<std::uint8_t,256> valid{};Remapper::ReadNative(s,false,values,valid);auto original=s;Remapper::WriteNative(s,false,values,values);Check(memcmp(&s,&original,sizeof(s))==0,"all native raw values preserve linear passthrough");}
 {IC_NATIVE_GAMEPAD_STATE s{};s.left_y=32767;std::array<int,256> values{};std::array<std::uint8_t,256> valid{};Remapper::ReadNative(s,false,values,valid);Check(values[1]==-100,"Cronus Y negative means up");auto output=values;output[1]=100;Remapper::WriteNative(s,false,values,output);Check(s.left_y==-32768,"positive GPC Y goes down in Windows native");}
 {Remapper::HidSlots slots;slots.PlayStation(true);slots.Seed({{1,0x30,0,0,0,1,1},{1,0x31,0,1,0,1,1},{1,0x32,0,2,0,1,1},{1,0x35,0,3,0,1,1}});Interceptor::Controller_Hid_Input in;Interceptor::Hid_Value a;a.usage_page=1;a.usage=0x30;a.normalized_valid=true;a.normalized=.5;a.minimum=0;a.maximum=255;in.state.axes.push_back(a);a.usage=0x32;a.normalized=-.5;in.state.axes.push_back(a);Interceptor::Hid_Button b;b.usage_page=9;b.usage=2;b.pressed=true;in.state.buttons.push_back(b);b.usage=3;b.pressed=false;in.state.buttons.push_back(b);std::array<int,256> values{};std::array<std::uint8_t,256> valid{};slots.Read(in,values,valid);Check(values[0]==50&&values[2]==-50&&values[10]==100&&values[11]==0,"Sony descriptor axes and face positions");auto output=values;output[10]=0;output[11]=100;output[2]=75;slots.Write(in,values,output);Check(!in.state.buttons[0].pressed&&in.state.buttons[1].pressed&&in.state.axes[1].normalized==.75,"Sony HID GPC controls are written back");}
 {auto v=ScriptCompiler::Compile("main{set_val(TRACE_1,123);}")->CreateInstance();auto f=Frame();f.Physical[208]=77;v->Execute(f,0,0);Check(f.Values[208]==77&&v->Debug().Traces[0]==123,"debug traces never alter physical HID vendor fields");}
 {auto v=ScriptCompiler::Compile("main{if(event_press(XB1_A))combo_run(C);}combo C{set_val(TRACE_4,65535);wait(20);}")->CreateInstance();auto f=Frame();f.Physical[10]=100;v->Execute(f,0,0);Check(v->Debug().Traces[3]==-1,"16-bit trace narrowing in combo");v->Execute(f,10000,10);Check(v->Debug().Traces[3]==-1,"held combo retains narrowed trace value");}
 Fault("main{while(TRUE){}}");Fault("int a[2];main{set_val(TRACE_1,a[2]);}");Fault("const int a[][]={{1,2},{3,4}};main{set_val(TRACE_1,a[0][2]);}");Fault("main{set_val(TRACE_1,1/0);}");
 Reject("main{set_val(XB1_A);}");Reject("main{unknown_hardware();}");Reject("main{\"text\";}");Reject("int x;int x;main{}");Reject("main{wait(10);}");
 {std::mt19937 fuzz(123);const char alphabet[]="int main{}()[];=+-*/0123456789abc_\n";for(int i=0;i<20000;++i){std::string source;for(unsigned j=0,n=fuzz()%160;j<n;++j)source+=alphabet[fuzz()%(sizeof(alphabet)-1)];try{(void)ScriptCompiler::Compile(source);}catch(const ScriptException&){}++checks;}}
 auto p=ScriptCompiler::Compile("main{if(event_press(XB1_A))combo_run(C);}combo C{set_val(XB1_B,100);wait(10);}");auto v=p->CreateInstance();auto f=Frame();std::mt19937 rng(42);auto begin=std::chrono::steady_clock::now();
 for(int i=0;i<200000;++i){for(unsigned j=0;j<22;++j)f.Physical[j]=j<4?static_cast<int>(rng()%201)-100:rng()%2?100:0;v->Execute(f,static_cast<long long>(i)*10000,10);for(int j=0;j<22;++j)Check(f.Values[j]>=-100&&f.Values[j]<=100,"bounded randomized controller stress");}
 auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/200000;
 std::cout<<"PASS "<<checks<<" semantic/range/stress checks. VM CPU-only mean "<<us<<" us/cycle, not end-to-end latency.\n";return 0;
 }catch(const ScriptException& e){std::cerr<<e.Diagnostic()<<"\n";return 1;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
