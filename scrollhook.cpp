#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <Windows.h>
#include <utility>
#include <cstdio>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string_view>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Kernel32.lib")

const DWORD defaultScanCode = 0x56;
const char *const defaultName = "ISO extra '\\'";

DWORD mainThreadId;
HHOOK hkKeybd = nullptr;
HHOOK hkMouse = nullptr;
bool swapMouseScroll = false;
std::queue<INPUT, std::list<INPUT>> *inputQueue;
std::mutex *mutex;
std::condition_variable *cond;

bool badCommandLine = false;
bool helpRequested = false;
bool verbose = false;
bool showKeyCodes = false;
bool swapScrollDirection = false;
DWORD targetScanCode = defaultScanCode;

template<class F>
struct ScopeExit {
  F f;
  ~ScopeExit() {
    f();
  }
};

template<class F>
ScopeExit<F> scope_exit(F f) {
  return ScopeExit<F>{std::move(f)};
}

void inputThread() {
  if (verbose) printf("Input queue started.\n");
  while (true) {
    std::unique_lock<std::mutex> lock{*mutex};
    cond->wait(lock);
    if (inputQueue->empty()) {
      if (verbose) printf("Input queue will shut down.\n");
      lock.unlock();
      cond->notify_one();
      return;
    }
    while (!inputQueue->empty()) {
      auto &input = inputQueue->back();
      SendInput(1, &input, sizeof(INPUT));
      inputQueue->pop();
      if (verbose) printf("Input queue processed 1 event.\n");
    }
  }
}

LRESULT CALLBACK KeybdLL(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(hkKeybd, nCode, wParam, lParam);
  }
  auto &info = *reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
  switch (wParam) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      if (showKeyCodes) {
        printf("Key down: VK = %ld; Scan = 0x%lX.\n",
               info.vkCode, info.scanCode);
        break;
      }
      if (info.scanCode == targetScanCode) {
        if (!swapMouseScroll) {
          if (verbose) printf("Intercepted toggle key down.\n");
          swapMouseScroll = true;
        }
        return true;
      }
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      if (showKeyCodes) {
        break;
      }
      if (info.scanCode == targetScanCode) {
        if (verbose) printf("Intercepted toggle key up.\n");
        swapMouseScroll = false;
        return true;
      }
      break;

    default:
      break;
  }
  return CallNextHookEx(hkKeybd, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseLL(int nCode, WPARAM wParam, LPARAM lParam) {
  auto &info = *reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
  if (nCode < 0 || showKeyCodes) {
    return CallNextHookEx(hkMouse, nCode, wParam, lParam);
  }
  switch (wParam) {
    case WM_MOUSEWHEEL:
      if (info.flags & LLMHF_INJECTED) { break; }
      if (swapMouseScroll) {
        INPUT input;
        input.type = INPUT_MOUSE;
        input.mi.dx = info.pt.x;
        input.mi.dy = info.pt.y;
        if (swapScrollDirection) {
          input.mi.mouseData = (DWORD)((long)info.mouseData >> 16);
        } else {
          input.mi.mouseData = (DWORD)(-((long)info.mouseData >> 16));
        }
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL | MOUSEEVENTF_ABSOLUTE;
        input.mi.time = info.time;
        input.mi.dwExtraInfo = 0;
        {
          std::lock_guard<std::mutex> guard{*mutex};
          inputQueue->emplace(input);
        }
        cond->notify_one();
        return true;
      }
      break;

    default:
      break;
  }
  return CallNextHookEx(hkMouse, nCode, wParam, lParam);
}

BOOL WINAPI CtrlHandler(DWORD event) {
  switch (event) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      if (verbose) printf("Posting quit message.\n");
      PostThreadMessage(mainThreadId, WM_QUIT, 0, 0);
      break;
    default:
      break;
  }
  return true;
}

void parseCommandLine(int argc, const wchar_t *const argv[]) {
  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg{argv[i]};
    if (arg == L"-h") {
      helpRequested = true;
    } else if (arg == L"-v") {
      verbose = true;
    } else if (arg == L"-s") {
      showKeyCodes = true;
    } else if (arg == L"-d") {
      swapScrollDirection = true;
    } else {
      long code;
      wchar_t *endptr;
      code = wcstol(argv[i], &endptr, 16);
      if ((endptr - argv[i]) != arg.size()) {
        printf("Invalid number '%ls'.\n", argv[i]);
        badCommandLine = true;
      } else {
        targetScanCode = (DWORD)code;
      }
    }
  }
}

void showHelp() {
  printf("Usage: scrollhook <options> <scan-code>\n");
  printf("Scan code specified in hex (no 0x).\n");
  printf("Default scan code = 0x%lX (%s).\n", defaultScanCode, defaultName);
  printf("Options:\n");
  printf("  -h  Show help.\n");
  printf("  -d  Swap scroll direction.\n");
  printf("  -s  Show scan codes instead of modifying input.\n");
  printf("  -v  Verbose logging.\n");
}

int wmain(int argc, wchar_t *argv[]) {
  HINSTANCE hMod = GetModuleHandle(nullptr);
  mainThreadId = GetCurrentThreadId();

  parseCommandLine(argc, argv);
  if (helpRequested || badCommandLine) {
    showHelp();
    return badCommandLine ? 1 : 0;
  }

  SetConsoleCtrlHandler(nullptr, false);
  SetConsoleCtrlHandler(CtrlHandler, true);

  inputQueue = new std::queue<INPUT, std::list<INPUT>>();
  mutex = new std::mutex();
  cond = new std::condition_variable();

  std::thread inputQueueThread{&inputThread};

  if (!(hkKeybd = SetWindowsHookEx(WH_KEYBOARD_LL, &KeybdLL, hMod, 0))) {
    return GetLastError();
  }
  if (!(hkMouse = SetWindowsHookEx(WH_MOUSE_LL,    &MouseLL, hMod, 0))) {
    return GetLastError();
  }
  if (verbose) printf("Hooks registered for scan code 0x%lX.\n", targetScanCode);

  auto iqfree = scope_exit([&]() {
    if (verbose) printf("Begin input queue shutdown.\n");
    {
      std::lock_guard<std::mutex> guard(*mutex);
      while (!inputQueue->empty()) {
        inputQueue->pop();
      }
    }
    if (verbose) printf("Waiting for input queue to finish.\n");
    cond->notify_one();
    {
      std::unique_lock<std::mutex> guard(*mutex);
      cond->wait(guard);
      delete inputQueue;
    }
    delete mutex;
    delete cond;
    inputQueueThread.join();
    if (verbose) printf("Input queue shut down finished.\n");
  });

  auto on_exit = scope_exit([&]() {
    if (hkMouse) UnhookWindowsHookEx(hkMouse);
    if (hkKeybd) UnhookWindowsHookEx(hkKeybd);
    if (verbose) printf("Hooks unregistered.\n");
  });

  if (verbose) printf("Message queue starting.\n");
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    DispatchMessage(&msg);
  }
  if (verbose) printf("Message queue ended.\n");
  return msg.wParam;
}
