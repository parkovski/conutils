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
const DWORD defaultPrecisionScanCode = 0x3B; // F1
const DWORD defaultHorizPrecisionScanCode = 0x3C; // F2

DWORD mainThreadId;
HHOOK hkKeybd = nullptr;
HHOOK hkMouse = nullptr;
bool swapMouseScroll = false;
bool precisionMode = false;
bool horizPrecisionMode = false;
std::queue<INPUT, std::list<INPUT>> *inputQueue;
std::mutex *mutex;
std::condition_variable *cond;

bool badCommandLine = false;
bool helpRequested = false;
bool verbose = false;
bool showKeyCodes = false;
bool swapScrollDirection = false;
DWORD targetScanCode = defaultScanCode;
DWORD precisionScanCode = defaultPrecisionScanCode;
DWORD horizPrecisionScanCode = defaultHorizPrecisionScanCode;

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
  std::unique_lock<std::mutex> lock{*mutex};
  if (verbose) printf("Input queue started.\n");
  while (true) {
    cond->wait(lock);
    if (inputQueue->empty()) {
      if (verbose) printf("Input queue will shut down.\n");
      lock.unlock();
      cond->notify_one();
      return;
    }
    int count = 0;
    do {
      auto &input = inputQueue->front();
      lock.unlock();
      SendInput(1, &input, sizeof(INPUT));
      ++count;
      lock.lock();
      inputQueue->pop();
    } while (!inputQueue->empty());
    if (verbose) printf("Input queue processed %d event(s).\n", count);
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
          if (verbose) printf("Toggle key down.\n");
          swapMouseScroll = true;
          precisionMode = false;
          horizPrecisionMode = false;
        }
        return true;
      } else if (info.scanCode == precisionScanCode) {
        if (!swapMouseScroll) {
          // If swapMouseScroll is false, this variable means "do we need to
          // eat the key up message" - aka was the key pressed while
          // swapMouseScroll was active.
          precisionMode = false;
        } else if (!precisionMode) {
          if (verbose) printf("Switching to precision mode.\n");
          precisionMode = true;
          horizPrecisionMode = false;
          return true;
        }
      } else if (info.scanCode == horizPrecisionScanCode) {
        if (!swapMouseScroll) {
          horizPrecisionMode = false;
        } else if (!horizPrecisionMode) {
          if (verbose) printf("Switching to horizontal precision mode.\n");
          horizPrecisionMode = true;
          precisionMode = false;
          return true;
        }
      }
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      if (showKeyCodes) {
        break;
      }
      if (info.scanCode == targetScanCode) {
        if (verbose) printf("Toggle key up.\n");
        swapMouseScroll = false;
        return true;
      } else if (info.scanCode == precisionScanCode && precisionMode) {
        if (verbose) printf("Ate precision mode key up.\n");
        if (!swapMouseScroll) {
          // Eat the first key up message but pass later ones on.
          precisionMode = false;
        }
        return true;
      } else if (info.scanCode == horizPrecisionScanCode && horizPrecisionMode) {
        if (verbose) printf("Ate horizontal precision mode key up.\n");
        if (!swapMouseScroll) {
          horizPrecisionMode = false;
        }
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
        input.mi.time = info.time;
        input.mi.dwExtraInfo = 0;
        if (precisionMode) {
          if ((long)info.mouseData > 0) {
            input.mi.mouseData = 40;
          } else {
            input.mi.mouseData = (DWORD)-40;
          }
          input.mi.dwFlags = MOUSEEVENTF_WHEEL | MOUSEEVENTF_ABSOLUTE;
        } else {
          if (swapScrollDirection) {
            input.mi.mouseData = (DWORD)((long)info.mouseData >> 16);
          } else {
            input.mi.mouseData = (DWORD)(-((long)info.mouseData >> 16));
          }
          input.mi.dwFlags = MOUSEEVENTF_HWHEEL | MOUSEEVENTF_ABSOLUTE;
        }
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

  if (!(hkKeybd = SetWindowsHookEx(WH_KEYBOARD_LL, &KeybdLL, hMod, 0))) {
    return GetLastError();
  }
  if (!(hkMouse = SetWindowsHookEx(WH_MOUSE_LL,    &MouseLL, hMod, 0))) {
    return GetLastError();
  }
  if (verbose) printf("Hooks registered for scan code 0x%lX, precision code 0x%lX, horizontal precision code 0x%lX.\n",
                      targetScanCode, precisionScanCode, horizPrecisionScanCode);

  std::thread inputQueueThread{&inputThread};

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
