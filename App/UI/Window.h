#pragma once
#include "../Capture/Service.h"
#include <Windows.h>
#include <filesystem>
namespace ci {
class Window {
 HWND window_=nullptr,editor_=nullptr,library_=nullptr,diagnostics_=nullptr,device_=nullptr,mode_=nullptr;
 HMODULE rich_=nullptr;HFONT font_=nullptr,codeFont_=nullptr,titleFont_=nullptr;HBRUSH background_=nullptr,panel_=nullptr;
 gpr::RemapperService service_;gpr::ServiceSnapshot snapshot_;std::vector<std::filesystem::path> files_;
 std::filesystem::path libraryPath_,active_;std::shared_ptr<const gpr::ScriptProgram> program_;
 bool offline_=false,dirty_=false,loading_=false;int width_=1360,height_=920,hover_=0,hoverFrom_=0;ULONGLONG hoverTime_=0;
 std::wstring log_=L"Open a .gpc source script, compile (F7), then run (F5).",compiledSource_;
 std::vector<std::pair<int,RECT>> actions_;
 void Layout();void Paint(HDC);void RefreshLibrary();void Open(const std::filesystem::path&);void Action(int);void Compile(bool);void Save();void Status(const std::wstring&);void UpdateMonitor();
 std::wstring Text()const;LRESULT Message(UINT,WPARAM,LPARAM);static LRESULT CALLBACK Proc(HWND,UINT,WPARAM,LPARAM);
public:
 explicit Window(bool offline=false):offline_(offline){}~Window();HWND Create(bool show=true);int Run();unsigned SelfTest();void Render(const std::wstring&);
};
}
