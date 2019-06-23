#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <string>
#include <string_view>
#include <sstream>

static HANDLE hStdin;
static HANDLE hStdout;

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
    if (rec.Event.FocusEvent.bSetFocus) {
      s << L"> Focus";
    } else {
      s << L"> Unfocus";
    }
    break;
  case KEY_EVENT:
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
    {
      s << L"> Menu #";
      s << rec.Event.MenuEvent.dwCommandId;
    }
    break;
  case MOUSE_EVENT:
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
    {
      const auto &e = rec.Event.WindowBufferSizeEvent;
      s << L"> BufferSize = " << e.dwSize.X << L" x " << e.dwSize.Y;
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

  newInMode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;

  if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'v' && argv[1][2] == 0) {
    newInMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
  }

  GetConsoleMode(hStdin, &inMode);
  SetConsoleMode(hStdin, newInMode);

  GetConsoleMode(hStdout, &outMode);
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
          } else {
            ctrlCCount = 0;
          }
      }
      --evCount;
    }
  }

  SetConsoleMode(hStdin, inMode);
  SetConsoleMode(hStdout, outMode);

  return 0;
}
