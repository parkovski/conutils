#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iostream>
#include <sstream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: hresult <number>\n";
    return 1;
  }

  std::stringstream ss;
  DWORD messageId;
  if (argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X')) {
    ss << std::hex << &argv[1][2];
  } else {
    ss << argv[1];
  }
  ss >> messageId;

  char buffer[1024];

  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, messageId, 0, buffer, 1024, nullptr);

  std::cout << buffer;

  return 0;
}
