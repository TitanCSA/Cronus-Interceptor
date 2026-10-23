#include "DriverSetup.h"
#include "../Interceptor/Interceptor.h"
#include <commctrl.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <system_error>
#include <vector>

namespace DriverSetup {
namespace {
constexpr auto InstallerHash="949ea2cd7d1b624ba6f9a830c1b95f77356d523cf9bf2fd86408a5888f8384c2";
constexpr auto Download=L"https://github.com/TitanCSA/Interceptor-Gamepad-Controller/releases/latest/download/InterceptorSetup.exe";
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct Hash {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    std::vector<UCHAR> object;
    ~Hash(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
};
std::wstring WindowsError(DWORD error){
    wchar_t text[512]{};FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,text,512,nullptr);
    return L"Windows error "+std::to_wstring(error)+L": "+text;
}
void Crypto(NTSTATUS result){if(result<0)throw std::runtime_error("Could not verify the driver installer SHA-256.");}
std::string Digest(HANDLE file){
    Hash context;Crypto(BCryptOpenAlgorithmProvider(&context.algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
    DWORD size=0,bytes=0;Crypto(BCryptGetProperty(context.algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&bytes,0));
    context.object.resize(size);Crypto(BCryptCreateHash(context.algorithm,&context.hash,context.object.data(),size,nullptr,0,0));
    std::array<UCHAR,65536> buffer{};DWORD read=0;
    for(;;){if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&read,nullptr))throw std::system_error(GetLastError(),std::system_category(),"Read driver installer");if(!read)break;Crypto(BCryptHashData(context.hash,buffer.data(),read,0));}
    std::array<UCHAR,32> digest{};Crypto(BCryptFinishHash(context.hash,digest.data(),static_cast<ULONG>(digest.size()),0));
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
}
Status Inspect(){
    DWORD error=ERROR_SUCCESS;
    try{
        const auto capabilities=Interceptor::Get_Capabilities();
        if(capabilities.input_mask==0&&capabilities.controller_mask==15)
            return {State::Available,0,L"Controller driver is available",L"Interceptor Gamepad & Controller responds. This check does not start capture or the demo allowance."};
        return {State::Incompatible,ERROR_REVISION_MISMATCH,L"The installed driver edition is incompatible",L"Install the Gamepad & Controller edition using InterceptorSetup.exe, then restart Windows when requested."};
    }catch(const std::system_error& e){error=static_cast<DWORD>(e.code().value());}
    if(error==ERROR_ACCESS_DENIED)return {State::AccessDenied,error,L"Controller driver access was denied",L"Run this application as administrator. An access error does not mean the driver is missing.\n\n"+WindowsError(error)};
    const auto manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(!manager){const auto code=GetLastError();return {State::Unavailable,code,L"Could not check the controller driver",WindowsError(code)};}
    const auto service=OpenServiceW(manager,L"InterceptorGamepadController",SERVICE_QUERY_STATUS);
    const auto serviceError=service?ERROR_SUCCESS:GetLastError();
    if(service)CloseServiceHandle(service);CloseServiceHandle(manager);
    if(serviceError==ERROR_SERVICE_DOES_NOT_EXIST&&(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND||error==ERROR_SERVICE_DOES_NOT_EXIST))
        return {State::Missing,error,L"Required controller driver is not installed",L"Install Interceptor Gamepad & Controller with the included InterceptorSetup.exe. Connect your controllers and close other remapping applications first.\n\nRestart Windows if setup requests it, reopen setup to finish any pending associations, then reopen this app. You can continue editing without the driver."};
    return {State::Unavailable,error,L"Controller driver needs setup or a restart",L"The controller driver could not be opened. Run InterceptorSetup.exe to install or repair the Gamepad & Controller edition. Restart Windows if requested, reopen setup to finish, then reopen this app.\n\n"+WindowsError(error)};
}
void OpenInstaller(HWND owner){
    wchar_t executable[32768]{};const auto count=GetModuleFileNameW(nullptr,executable,32768);
    if(!count||count>=32768)throw std::runtime_error("Could not locate the application folder.");
    std::wstring path(executable,count);path.resize(path.find_last_of(L"\\/")+1);path+=L"InterceptorSetup.exe";
    File file;file.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file.value==INVALID_HANDLE_VALUE){
        const auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)throw std::system_error(error,std::system_category(),"Open adjacent driver installer");
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(owner,L"open",Download,nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("Could not open the driver download. Download InterceptorSetup.exe from the app release.");return;
    }
    LARGE_INTEGER size{};if(!GetFileSizeEx(file.value,&size)||size.QuadPart<1||size.QuadPart>67108864)throw std::runtime_error("Invalid adjacent driver installer size.");
    if(Digest(file.value)!=InstallerHash)throw std::runtime_error("The adjacent InterceptorSetup.exe does not match this app's verified Gamepad & Controller 1.0.12.0 installer. Download the complete app ZIP again.");
    SHELLEXECUTEINFOW launch{sizeof(launch)};launch.fMask=SEE_MASK_NOASYNC;launch.hwnd=owner;launch.lpVerb=L"open";launch.lpFile=path.c_str();launch.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&launch))throw std::system_error(GetLastError(),std::system_category(),"Launch verified driver setup");
}
void Prompt(HWND owner,const Status& status){
    if(status.Available())return;
    const TASKDIALOG_BUTTON buttons[]={{100,L"Open driver setup"},{IDCANCEL,L"Continue without driver"}};
    TASKDIALOGCONFIG dialog{sizeof(dialog)};dialog.hwndParent=owner;dialog.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT;
    dialog.pszWindowTitle=L"Interceptor Gamepad & Controller";dialog.pszMainIcon=TD_INFORMATION_ICON;dialog.pszMainInstruction=status.title.c_str();dialog.pszContent=status.detail.c_str();dialog.cButtons=2;dialog.pButtons=buttons;dialog.nDefaultButton=100;
    int selected=IDCANCEL;
    if(FAILED(TaskDialogIndirect(&dialog,&selected,nullptr,nullptr))){const auto text=status.title+L"\n\n"+status.detail+L"\n\nOpen driver setup?";selected=MessageBoxW(owner,text.c_str(),L"Controller driver",MB_YESNO|MB_ICONINFORMATION)==IDYES?100:IDCANCEL;}
    if(selected==100){try{OpenInstaller(owner);}catch(const std::exception& e){MessageBoxA(owner,e.what(),"Driver setup",MB_OK|MB_ICONERROR);}}
}
void WriteReport(const std::wstring& path){
    const auto result=Inspect();std::ofstream out(path);
    out<<"{\"available\":"<<(result.Available()?"true":"false")<<",\"missing\":"<<(result.state==State::Missing?"true":"false")<<",\"state\":"<<static_cast<unsigned>(result.state)<<",\"error\":"<<result.error<<",\"captureStarted\":false}";
    if(!out)throw std::runtime_error("Could not write the driver-check report.");
}
}
