// Copyright 2016 Aether authors. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include "pch.h"
#include "../model/model.h"
#include <windows.h>
HINSTANCE hInstance;

namespace {
  // Convert a wide Unicode string to an UTF8 string
  std::string WcsToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str_to.data(), size_needed, NULL, NULL);
    return str_to;
  }

  std::wstring Utf8ToWcs(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr_to(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr_to.data(), size_needed);
    return wstr_to;
  }
}

class MainPresenterWin : public MainPresenter {
public:
  AETHER_OBJ(MainPresenterWin, MainPresenter);
  template <typename T> void Serializator(T& s) { }
  virtual bool OnEvent(const aether::Event::ptr& event);
  virtual void OnLoaded();
  void OnClick() {
    main_->PushEvent(new Event1());
  }
  HWND hWnd;
};

class DocWindowPresenterWin : public DocWindowPresenter {
public:
  AETHER_OBJ(DocWindowPresenterWin, DocWindowPresenter);
  template <typename T> void Serializator(T& s) {}
  virtual bool OnEvent(const aether::Event::ptr& event);
  virtual void OnLoaded();
  HWND hWnd_;
};


class TextPresenterWin : public TextPresenter {
public:
  AETHER_OBJ(TextPresenterWin, TextPresenter);
  template <typename T> void Serializator(T& s) {}
  virtual bool OnEvent(const aether::Event::ptr& event);
  virtual void OnLoaded();
  HWND hWnd_;
};

//static std::map<HWND, aether::Obj*> presenters_;
//static std::map<int, HWND> id_to_hwnd_;
HWND hwndButton;
MainPresenterWin* main_presenter;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_COMMAND: {
    switch (HIWORD(wParam)) {
    case BN_CLICKED:
      if ((HWND)lParam == hwndButton) {
        main_presenter->OnClick();
      }
      break;
    }
    break;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainPresenterWin::OnLoaded() {
  const wchar_t kWndClassName[] = L"AetherDocMainWindowClass";
  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWndClassName;
  RegisterClass(&wc);
  hWnd = CreateWindowEx(0, kWndClassName, L"Aether::Doc", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, NULL, NULL, hInstance, NULL);
  if (hWnd == NULL) return;

  hwndButton = CreateWindow(L"BUTTON", L"OK", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
    10, 10, 100, 100, hWnd, NULL, hInstance, NULL);
  main_presenter = this;
  ShowWindow(hWnd, SW_SHOW);
}

bool MainPresenterWin::OnEvent(const aether::Event::ptr& event) {
  switch (event->GetId()) {
  default:
    return aether::Obj::OnEvent(event);
  }
}
AETHER_IMPL(MainPresenterWin);



DocWindowPresenterWin* doc_presenter;

LRESULT CALLBACK DocWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    //PostQuitMessage(0);
    return 0;
  case WM_COMMAND: {
    switch (HIWORD(wParam)) {
      /*    case EN_CHANGE: {
            HWND hWnd = id_to_hwnd_[LOWORD(wParam)];
            int num_symbols = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);
            std::wstring wide_string;
            wide_string.resize(num_symbols);
            SendMessage(hWnd, WM_GETTEXT, (WPARAM)num_symbols * sizeof(wchar_t), (LPARAM)wide_string.data());
            EventTextChanged::ptr e(new EventTextChanged(0, 0, WcsToUtf8(wide_string)));
            TextPresenterWin::ptr(presenters_[hWnd])->text_->PushEvent(e);
            break;
          }*/
    }
    break;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


void DocWindowPresenterWin::OnLoaded() {
  const wchar_t kWndClassName[] = L"AetherDocDocWindowClass";
  WNDCLASS wc = {};
  wc.lpfnWndProc = DocWindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = kWndClassName;
  RegisterClass(&wc);
  hWnd_ = CreateWindowEx(0, kWndClassName, L"Aether::DocWindow", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, NULL, NULL, hInstance, NULL);
  //if (hWnd_ == NULL) return;

  doc_presenter = this;
  ShowWindow(hWnd_, SW_SHOW);
}

bool DocWindowPresenterWin::OnEvent(const aether::Event::ptr& event) {
  switch (event->GetId()) {
  default:
    return aether::Obj::OnEvent(event);
  }
}
AETHER_IMPL(DocWindowPresenterWin);

void TextPresenterWin::OnLoaded() {
//  HWND hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), TEXT("test"), WS_CHILD | WS_VISIBLE | WS_VSCROLL |
    //ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, 10, 10, 140, 60, presenters_.begin()->first, (HMENU)123, NULL, NULL);
  HWND hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("Edit"), TEXT("test"), WS_CHILD | WS_VISIBLE | WS_VSCROLL |
    ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, 10, 10, 140, 60, 0, (HMENU)123, NULL, NULL);
  SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Utf8ToWcs(text_->string_).c_str());
  //id_to_hwnd_[123] = hWnd;
  //presenters_[hWnd] = this;
}

bool TextPresenterWin::OnEvent(const aether::Event::ptr& event) {
  switch (event->GetId()) {
  case EventTextChanged::kId: {
    // Text is already inserted by the window so do nothing
    return false;
  }
  default:
    return aether::Obj::OnEvent(event);
  }
}
AETHER_IMPL(TextPresenterWin);


#ifdef _DEBUG
int __cdecl main() {
  hInstance = GetModuleHandle(NULL);
#else
int APIENTRY wWinMain(HINSTANCE hInstance1, HINSTANCE, LPWSTR, int) {
  hInstance = hInstance1;
#endif
  App::ptr app = App::Create("state");
  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  App::Release(std::move(app));
  return 0;
}
