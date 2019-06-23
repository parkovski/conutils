#include <Windows.h>
#include <sstream>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Usage: resize <pid>\n";
    return 1;
  }

  DWORD pid;
  std::stringstream ss;
  ss << argv[1];
  ss >> pid;

  if (!pid) {
    std::cout << "Invalid pid.\n";
    return 1;
  }

  FreeConsole();

  if (!AttachConsole(pid)) {
    std::cout << "AttachConsole failed.\n";
    return GetLastError();
  }

  CONSOLE_SCREEN_BUFFER_INFO sbi;
  if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &sbi)) {
    std::cout << "GetConsoleScreenBufferInfo failed.\n";
    return GetLastError();
  }

  INPUT_RECORD rec;
  rec.EventType = WINDOW_BUFFER_SIZE_EVENT;
  // rec.Event.WindowBufferSizeEvent.dwSize = {
  //   sbi.srWindow.Right - sbi.srWindow.Left,
  //   sbi.srWindow.Bottom - sbi.srWindow.Top,
  // };
  rec.Event.WindowBufferSizeEvent.dwSize = sbi.dwSize;

  SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),
                             sbi.dwSize);

  DWORD numEvents;
  if (!WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &rec, 1, &numEvents)) {
    std::cout << "WriteConsoleInput failed.\n";
    return GetLastError();
  }

  return 0;
}
