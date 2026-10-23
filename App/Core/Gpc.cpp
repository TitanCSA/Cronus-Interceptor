#include "Gpc.h"
#include <unordered_map>
namespace gpr {
const std::array<int,48>& GpcSlots(){
 static const std::array<int,48> slots{20,16,17,15,5,19,14,4,18,2,3,0,1,6,7,8,9,13,11,10,12,-1,
  -1,-1,-1,-1,-1,21,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,208,209,210,211,212,213,2,3,0,1};return slots;
}
int GpcConstant(const std::string& name){
 static const auto constants=[] {
  std::unordered_map<std::string,int> m;
  const char* xbox[]{"XBOX","VIEW","MENU","RB","RT","RS","LB","LT","LS","RX","RY","LX","LY","UP","DOWN","LEFT","RIGHT","Y","B","A","X","SHARE"};
  const char* ps[]{"PS","SHARE","OPTIONS","R1","R2","R3","L1","L2","L3","RX","RY","LX","LY","UP","DOWN","LEFT","RIGHT","TRIANGLE","CIRCLE","CROSS","SQUARE","TOUCH"};
  for(int i=0;i<22;++i){for(const char* p:{"XB1_","XB360_","XSX_"})m[std::string(p)+xbox[i]]=i;for(const char* p:{"PS3_","PS4_","PS5_"})m[std::string(p)+ps[i]]=i;}
  m["XB1_GUIDE"]=m["XB360_GUIDE"]=0;m["XB360_BACK"]=1;m["XB360_START"]=2;m["PS3_SELECT"]=1;m["PS3_START"]=2;m["PS5_CREATE"]=1;
  const char* sw[]{"HOME","MINUS","PLUS","R","ZR","RS","L","ZL","LS","RX","RY","LX","LY","UP","DOWN","LEFT","RIGHT","X","A","B","Y","CAPTURE"};
  for(int i=0;i<22;++i)m[std::string("SWITCH_")+sw[i]]=i;
  for(const char* p:{"PS3_","PS4_","PS5_"}){m[std::string(p)+"ACCX"]=22;m[std::string(p)+"ACCY"]=23;m[std::string(p)+"ACCZ"]=24;m[std::string(p)+"GYROX"]=25;m[std::string(p)+"GYROY"]=26;m[std::string(p)+"GYROZ"]=27;}
  for(const char* p:{"PS3_","PS4_","PS5_"}){m[std::string(p)+"ACCX"]=21;m[std::string(p)+"ACCY"]=22;m[std::string(p)+"ACCZ"]=23;m[std::string(p)+"GYROX"]=24;m[std::string(p)+"GYROY"]=25;m[std::string(p)+"GYROZ"]=26;}
  m["PS3_GYRO"]=24;m["PS4_TOUCH"]=m["PS5_TOUCH"]=27;m["PS4_TOUCHX"]=28;m["PS4_TOUCHY"]=29;
  const char* wii[]{"HOME","MINUS","PLUS","RT","ZR","ONE","C","Z","TWO","RX","RY","LX","LY","UP","DOWN","LEFT","RIGHT","X","B","A","Y","ACCX","ACCY","ACCZ","GYRO","ACCNX","ACCNY","ACCNZ","IRX","IRY"};
  for(int i=0;i<30;++i)m[std::string("WII_")+wii[i]]=i;
  for(int i=1;i<=6;++i)m["TRACE_"+std::to_string(i)]=37+i;
  for(int i=1;i<=64;++i)m["SPVAR_"+std::to_string(i)]=i-1;
  for(int i=1;i<=16;++i)m["PVAR_"+std::to_string(i)]=63+i;
  m["POLAR_RS"]=2;m["POLAR_LS"]=4;m["POLAR_RADIUS"]=0;m["POLAR_ANGLE"]=1;
  m["POLAR_RX"]=44;m["POLAR_RY"]=45;m["POLAR_LX"]=46;m["POLAR_LY"]=47;
  m["PIO_NONE"]=m["PIO_AUTO"]=0;m["PIO_PS3"]=1;m["PIO_XB360"]=2;m["PIO_WII"]=3;m["PIO_PS4"]=4;m["PIO_XB1"]=5;m["PIO_SWITCH"]=6;m["PIO_PS5"]=7;
  m["DZ_CIRCLE"]=101;m["NOT_USE"]=-1;
  m["LED_1"]=0;m["LED_2"]=1;m["LED_3"]=2;m["LED_4"]=3;
  m["LED_OFF"]=0;m["LED_ON"]=1;m["LED_BLINK"]=2;
  m["RUMBLE_A"]=0;m["RUMBLE_B"]=1;m["RUMBLE_LT"]=2;m["RUMBLE_RT"]=3;
  return m;
 }();const auto p=constants.find(name);return p==constants.end()?-2147483647:p->second;
}
}
