#pragma once
#include <Windows.h>
#include <string>

namespace DriverSetup {
enum class State { Available, Missing, Unavailable, Incompatible, AccessDenied };
struct Status {
    State state=State::Unavailable;
    DWORD error=0;
    std::wstring title,detail;
    bool Available() const noexcept {return state==State::Available;}
};
// Metadata only: no initialization, context, subscription or demo activation.
Status Inspect();
void OpenInstaller(HWND owner);
void Prompt(HWND owner,const Status& status);
void WriteReport(const std::wstring& path);
}
