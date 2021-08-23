#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>

enum EventMask {
  EM_Keyboard     = 0x1,
  EM_Mouse        = 0x2,
  EM_Focus        = 0x4,
  EM_Menu         = 0x8,
  EM_WindowSize   = 0x10,
  EM_Char         = 0x20,
  EM_Hex          = 0x40,
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
    s << L"\r\n  Mod =";
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
    if (!(eventMask & (EM_Keyboard | EM_Char | EM_Hex))) {
      return;
    }
    {
      const auto &e = rec.Event.KeyEvent;
      auto uchar = (unsigned short)e.uChar.UnicodeChar;
      if (eventMask & EM_Keyboard) {
        s << L"> Key" << (e.bKeyDown ? L" down" : L" up");
        s << L"\r\n  Unicode = 0x" << std::hex << std::uppercase << std::setfill(L'0');
        if (uchar & 0xFF00) {
          s << std::setw(4);
        } else {
          s << std::setw(2);
        }
        s << uchar << std::dec << std::nouppercase << std::setw(0) << L" = '";
      } else if (!e.bKeyDown) {
        return;
      }
      if (eventMask == EM_Hex) {
        s << std::hex << std::setfill(L'0') << std::setw(2);
        if (uchar & 0xFF00) {
          s << (uchar >> 8) << std::setw(0) << L'_' << std::setw(2);
          uchar &= 0xFF;
        }
        s << uchar << L' ';
        write(s.str());
        return;
      }
      if (e.uChar.UnicodeChar < 32) {
        s << L"\x1b[31m"
          << L'^' << static_cast<wchar_t>(uchar + 'A' - 1)
          << L"\x1b[m";
      } else {
        s << e.uChar.UnicodeChar;
      }
      if (eventMask == EM_Char) {
        write(s.str());
        return;
      }
      s << L'\'';
      ccs(e.dwControlKeyState);
      s << L"\r\n  VK = 0x" << std::hex << std::uppercase << e.wVirtualKeyCode
        << L" / " << std::dec << e.wVirtualKeyCode << std::nouppercase;
      s << L"\r\n  Scan = 0x" << std::hex << std::uppercase << e.wVirtualScanCode
        << L" / " << std::dec << e.wVirtualScanCode << std::nouppercase;
      if (e.wRepeatCount > 1) {
        s << L"\r\n  Repeat = " << e.wRepeatCount;
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
      s << L"\r\n  Buttons = 0x" << std::hex << std::uppercase << e.dwButtonState
        << std::nouppercase;
      ccs(e.dwControlKeyState);
      s << L"\r\n  EventFlags = 0x" << std::uppercase << e.dwEventFlags
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
      s << L"\r\n> ScreenBufferInfo:"
        << L"\r\n    Size = " << sbi.dwSize.X << " x " << sbi.dwSize.Y
        << L"\r\n    Window = [(" << sbi.srWindow.Left << L", " << sbi.srWindow.Top
        << L"), (" << sbi.srWindow.Right << L", " << sbi.srWindow.Bottom << L")]"
        << L"\r\n    MaxWindowSize = " << sbi.dwMaximumWindowSize.X << L" x "
        << sbi.dwMaximumWindowSize.Y;
    }
    break;
  }

  s << L"\r\n\n";
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
  bool useReadFile = false;
  bool useReadConsole = false;

  newInMode = ENABLE_EXTENDED_FLAGS;

  eventMask = 0;

  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    if (arg == L"-v") {
      newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    } else if (arg == L"-ba") {
      altbuffer = AltBufferApi;
    } else if (arg == L"-bv") {
      altbuffer = AltBufferVt;
    } else if (arg == L"-rf") {
      useReadFile = true;
    } else if (arg == L"-rc") {
      useReadConsole = true;
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
          case 'c':
            eventMask |= EM_Char;
            break;
          case 'x':
            eventMask |= EM_Hex;
            break;
          default:
            wchar_t e[2] = { arg[i], 0 };
            write(L"Unknown event type '");
            write(e);
            write(L"'\r\n");
            return 1;
        }
      }
      if ((eventMask & EM_Char) && eventMask != EM_Char) {
        write(L"-ec not valid with other -e options.\r\n");
        return 1;
      }
      if ((eventMask & EM_Hex) && eventMask != EM_Hex) {
        write(L"-ex not valid with other -e options.\r\n");
        return 1;
      }
      if ((useReadFile || useReadConsole) && (eventMask & ~(EM_Hex | EM_Char))) {
        write(L"-r* only valid with -ec and -ex.\r\n");
        return 1;
      }
      if (useReadFile && useReadConsole) {
        write(L"only one of -rf and -rc can be used.\r\n");
        return 1;
      }
    } else if (arg == L"-h") {
      write(L"Args:\r\n"
            L"  -h        = help\r\n\n"

            L"  -v        = virtual terminal input\r\n"
            L"  -bv       = alt buffer through VT\r\n"
            L"  -ba       = alt buffer through console API\r\n"
            L"  -e[fkmus] = Enable event types (focus, key, mouse, menu, size)\r\n"
            L"              Default is keyboard and window size events\r\n\n"

            L"              The following options cannot be combined with other -e options:\r\n"
            L"  -ec       = Just print characters\r\n"
            L"  -ex       = Just print hex\r\n"
            L"  -rf       = Use ReadFile instead of ReadConsoleInput.\r\n"
            L"  -rc       = Use ReadConsole instead of ReadConsoleInput.\r\n");
      return 0;
    } else {
      write(L"Unknown arg '");
      write(arg);
      write(L"'.\r\n");
      return 1;
    }
  }
  if (!eventMask) {
    if (useReadFile || useReadConsole) {
      eventMask = EM_Char;
    } else {
      eventMask = EM_Keyboard | EM_WindowSize;
    }
  }

  GetConsoleMode(hStdout, &outMode);
  SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT |
                          ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);

  HANDLE hOldStdout = nullptr;
  if (altbuffer == AltBufferApi) {
    hOldStdout = hStdout;
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
    SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT |
                            ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
    SetConsoleActiveScreenBuffer(hStdout);
  } else if (altbuffer == AltBufferVt) {
    write(L"\x1b[?1049h");
  }

  GetConsoleMode(hStdin, &inMode);
  if (eventMask & EM_Mouse) {
    newInMode |= ENABLE_MOUSE_INPUT;
  }
  if (eventMask & EM_WindowSize) {
    newInMode |= ENABLE_WINDOW_INPUT;
  }
  if (eventMask & (EM_Char | EM_Hex)) {
    eventMask &= EM_Char | EM_Hex;
    if (newInMode & ENABLE_VIRTUAL_TERMINAL_INPUT) {
      newInMode |= ENABLE_MOUSE_INPUT;
    }
  }
  SetConsoleMode(hStdin, newInMode);

  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);

  write(L"Press ^C twice to exit.\r\n\n");

  int ctrlCCount = 0;
  if (useReadFile || useReadConsole) {
    char buf[64];
    DWORD nbytes;
    INPUT_RECORD rec{};
    while (true) {
      if (useReadConsole) {
        CONSOLE_READCONSOLE_CONTROL inpctl{};
        if (!ReadConsoleA(hStdin, buf, sizeof(buf), &nbytes, &inpctl)) {
          return GetLastError();
        }
      } else {
        if (!ReadFile(hStdin, buf, sizeof(buf), &nbytes, nullptr)) {
          return GetLastError();
        }
      }
      if (!nbytes) {
        continue;
      }
      for (DWORD i = 0; i < nbytes; ++i) {
        char c = buf[i];
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.uChar.UnicodeChar = (WCHAR)c;
        rec.Event.KeyEvent.bKeyDown = true;
        printev(rec);
        if (c == 3) {
          if (ctrlCCount == 1) {
            goto read_finished;
          } else {
            ctrlCCount = 1;
          }
        } else {
          ctrlCCount = 0;
        }
      }
      if (eventMask == EM_Char) {
        write(L" ");
      }
    }
    read_finished:;
  } else {
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
        if (eventMask == EM_Char && evCount == 1 && rec.EventType == KEY_EVENT) {
          write(L" ");
        }
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
  }

  if (altbuffer == AltBufferApi) {
    SetConsoleActiveScreenBuffer(hOldStdout);
    CloseHandle(hStdout);
    hStdout = hOldStdout;
  } else if (altbuffer == AltBufferVt) {
    write(L"\x1b[?1049l");
  }

  SetConsoleMode(hStdin, inMode);
  SetConsoleMode(hStdout, outMode);

  return 0;
}
