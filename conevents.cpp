#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <string>
#include <string_view>
#include <sstream>

enum EventMask {
  EM_Keyboard     = 0x1,
  EM_Mouse        = 0x2,
  EM_Focus        = 0x4,
  EM_Menu         = 0x8,
  EM_WindowSize   = 0x10,
};

static HANDLE hStdin;
static HANDLE hStdout;
static unsigned eventMask;

static void write(std::wstring_view str) {
  DWORD charsWritten;
  WriteConsoleW(hStdout, str.data(), str.length(), &charsWritten, nullptr);
}

static void printev(const INPUT_RECORD &rec) {
  std::wstringstream s{};

  auto ccs = [&s](DWORD dwControlKeyState) {
    if (!dwControlKeyState) {
      return;
    }
    s << L"\n  Mod =";
    if (dwControlKeyState & ENHANCED_KEY) {
      s << L" Enh";
    }
    if (dwControlKeyState & SHIFT_PRESSED) {
      s << L" Shift";
    }
    if (dwControlKeyState & LEFT_CTRL_PRESSED) {
      s << L" LCtrl";
    }
    if (dwControlKeyState & RIGHT_CTRL_PRESSED) {
      s << L" RCtrl";
    }
    if (dwControlKeyState & LEFT_ALT_PRESSED) {
      s << L" LAlt";
    }
    if (dwControlKeyState & RIGHT_ALT_PRESSED) {
      s << L" RAlt";
    }
    if (dwControlKeyState & CAPSLOCK_ON) {
      s << L" Caps";
    }
    if (dwControlKeyState & NUMLOCK_ON) {
      s << L" Num";
    }
    if (dwControlKeyState & SCROLLLOCK_ON) {
      s << L" Scroll";
    }
    s << L" (0x" << std::hex << std::uppercase << dwControlKeyState << std::dec << std::nouppercase << L")";
  };

  switch (rec.EventType) {
  case FOCUS_EVENT:
    if (!(eventMask & EM_Focus)) {
      return;
    }
    if (rec.Event.FocusEvent.bSetFocus) {
      s << L"> Focus";
    } else {
      s << L"> Unfocus";
    }
    break;
  case KEY_EVENT:
    if (!(eventMask & EM_Keyboard)) {
      return;
    }
    {
      const auto &e = rec.Event.KeyEvent;
      //if (!e.bKeyDown) { return; }
      s << L"> Key" << (e.bKeyDown ? L" down" : L" up");
      s << L"\n  Unicode = 0x" << std::hex << std::uppercase <<
        (unsigned short)e.uChar.UnicodeChar << std::dec << std::nouppercase;
      if (e.uChar.UnicodeChar == 0) {
        //s << " ^@";
      } else if (e.uChar.UnicodeChar < 32) {
        s << " ^" << static_cast<wchar_t>(e.uChar.UnicodeChar + 'A' - 1);
      } else {
        wchar_t ch[] = { L' ', L'\'', e.uChar.UnicodeChar, L'\'', 0 };
        s << ch;
      }
      ccs(e.dwControlKeyState);
      s << L"\n  VK = 0x" << std::hex << std::uppercase << e.wVirtualKeyCode
        << L" / " << std::dec << e.wVirtualKeyCode << std::nouppercase;
      s << L"\n  Scan = 0x" << std::hex << std::uppercase << e.wVirtualScanCode
        << L" / " << std::dec << e.wVirtualScanCode << std::nouppercase;
      if (e.wRepeatCount > 1) {
        s << L"\n  Repeat = " << e.wRepeatCount;
      }
    }
    break;
  case MENU_EVENT:
    if (!(eventMask & EM_Menu)) {
      return;
    }
    {
      s << L"> Menu #";
      s << rec.Event.MenuEvent.dwCommandId;
    }
    break;
  case MOUSE_EVENT:
    if (!(eventMask & EM_Mouse)) {
      return;
    }
    {
      const auto &e = rec.Event.MouseEvent;
      s << L"> Mouse (" << e.dwMousePosition.X << L", " << e.dwMousePosition.Y
        << L")";
      s << L"\n  Buttons = 0x" << std::hex << std::uppercase << e.dwButtonState
        << std::nouppercase;
      ccs(e.dwControlKeyState);
      s << L"\n  EventFlags = 0x" << std::uppercase << e.dwEventFlags
        << std::nouppercase;
    }
    break;
  case WINDOW_BUFFER_SIZE_EVENT:
    if (!(eventMask & EM_WindowSize)) {
      return;
    }
    {
      const auto &e = rec.Event.WindowBufferSizeEvent;
      s << L"> BufferSize = " << e.dwSize.X << L" x " << e.dwSize.Y;
      CONSOLE_SCREEN_BUFFER_INFO sbi;
      GetConsoleScreenBufferInfo(hStdout, &sbi);
      s << L"\n> ScreenBufferInfo:"
        << L"\n    Size = " << sbi.dwSize.X << " x " << sbi.dwSize.Y
        << L"\n    Window = [(" << sbi.srWindow.Left << L", " << sbi.srWindow.Top
        << L"), (" << sbi.srWindow.Right << L", " << sbi.srWindow.Bottom << L")]"
        << L"\n    MaxWindowSize = " << sbi.dwMaximumWindowSize.X << L" x "
        << sbi.dwMaximumWindowSize.Y;
    }
    break;
  }

  s << L"\n\n";
  write(s.str());
}

int wmain(int argc, wchar_t *argv[]) {
  hStdin = GetStdHandle(STD_INPUT_HANDLE);
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD inMode, newInMode;
  DWORD outMode;
  enum {
    AltBufferNone = 0,
    AltBufferApi = 1,
    AltBufferVt = 2,
  };
  int altbuffer = AltBufferNone;

  newInMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;

  eventMask = EM_Keyboard | EM_WindowSize;

  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    if (arg == L"-v") {
      newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    } else if (arg == L"-ba") {
      altbuffer = AltBufferApi;
    } else if (arg == L"-bv") {
      altbuffer = AltBufferVt;
      newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    } else if (arg[0] == '-' && arg[1] == 'e') {
      eventMask = 0;
      for (int i = 2; i < arg.length(); ++i) {
        switch (arg[i]) {
          case 'f':
            eventMask |= EM_Focus;
            break;
          case 'k':
            eventMask |= EM_Keyboard;
            break;
          case 'm':
            eventMask |= EM_Mouse;
            break;
          case 'u':
            eventMask |= EM_Menu;
            break;
          case 's':
            eventMask |= EM_WindowSize;
            break;
          default:
            wchar_t e[2] = { arg[i], 0 };
            write(L"Unknown event type '");
            write(e);
            write(L"'\n");
            return 1;
        }
      }
    } else if (arg == L"-h") {
      write(L"Args:\n"
            L"  -v        = virtual terminal input\n"
            L"  -bv       = alt buffer through VT, implies -v.\n"
            L"  -ba       = alt buffer through console API\n"
            L"  -e[fkmus] = Enable event types (focus, key, mouse, menu, size)\n"
            L"              Default is keyboard and window size events\n"
            L"  -h        = help\n");
      return 0;
    } else {
      write(L"Unknown arg '");
      write(arg);
      write(L"'.\n");
      return 1;
    }
  }

  GetConsoleMode(hStdout, &outMode);

  if (altbuffer == AltBufferApi) {
    hStdout = CreateConsoleScreenBuffer(
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      CONSOLE_TEXTMODE_BUFFER,
      nullptr
    );
    if (!hStdout) {
      write(L"Creating screen buffer failed!");
    }
    SetConsoleActiveScreenBuffer(hStdout);
  } else if (altbuffer == AltBufferVt) {
    write(L"\x1b[?1049h");
  }

  GetConsoleMode(hStdin, &inMode);
  SetConsoleMode(hStdin, newInMode);
  SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

  write(L"Press ^C twice to exit.\n\n");

  int ctrlCCount = 0;
  while (ctrlCCount < 2) {
    DWORD evCount;
    if (!GetNumberOfConsoleInputEvents(hStdin, &evCount)) {
      Sleep(100);
      continue;
    }
    while (evCount) {
      DWORD numRead;
      INPUT_RECORD rec;
      ReadConsoleInputW(hStdin, &rec, 1, &numRead);
      if (numRead == 0) {
        break;
      }
      printev(rec);
      if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
          if (rec.Event.KeyEvent.uChar.UnicodeChar == 3) {
            ++ctrlCCount;
          } else if (rec.Event.KeyEvent.uChar.UnicodeChar) {
            ctrlCCount = 0;
          }
      }
      --evCount;
    }
  }

  if (altbuffer == AltBufferApi) {
    CloseHandle(hStdout);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  } else if (altbuffer == AltBufferVt) {
    write(L"\x1b[?1049l");
  }

  SetConsoleMode(hStdin, inMode);
  SetConsoleMode(hStdout, outMode);

  return 0;
}
