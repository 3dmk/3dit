// Windows-only native title bar styling; no effect on ImGui docking.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#endif
void ApplyN3DLiteTitlebarTheme(){
#ifdef _WIN32
 HWND hwnd=GetActiveWindow();
 if(!hwnd)return;
 HMODULE dwm=LoadLibraryW(L"dwmapi.dll");
 if(!dwm)return;
 using SetAttribute=HRESULT (WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
 auto set=(SetAttribute)GetProcAddress(dwm,"DwmSetWindowAttribute");
 if(set){
  const BOOL dark=TRUE;
  // Windows 10 20H1+ uses 20; older Windows 10 uses 19.
  if(FAILED(set(hwnd,20,&dark,sizeof(dark))))set(hwnd,19,&dark,sizeof(dark));
  const COLORREF bg=RGB(38,38,38);
  const COLORREF fg=RGB(224,224,224);
  const COLORREF border=RGB(65,65,65);
  // Windows 11 native caption colors; ignored gracefully on Windows 10.
  set(hwnd,35,&bg,sizeof(bg));
  set(hwnd,36,&fg,sizeof(fg));
  set(hwnd,34,&border,sizeof(border));
 }
 FreeLibrary(dwm);
#endif
}
