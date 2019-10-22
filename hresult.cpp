#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winternl.h>

#include <iostream>
#include <sstream>

#define REQUIRE_ARGS(_n) if (argc != (_n) + 1) { \
    std::cout << "Expected " << (_n) << "arguments.\n"; return 1; \
  } else { \
    argv[1] = argv[_n]; \
  }

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: hresult [nt] <number>.\n";
    std::cerr << "The nt option interprets NTSTATUS values.\n";
    std::cerr << "Numbers can be in decimal or hex (0x...) format.\n";
    return 1;
  }

  constexpr int cvt_none = 0;
  constexpr int cvt_nt = 1;

  int convert = cvt_none;
  if (!_strcmpi(argv[1], "nt")) {
    convert = cvt_nt;
    REQUIRE_ARGS(2)
  } else {
    REQUIRE_ARGS(1)
  }

  std::stringstream ss;
  DWORD messageId;
  if (argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X')) {
    ss << std::hex << &argv[1][2];
  } else {
    ss << argv[1];
  }
  ss >> messageId;

  std::cout << "Code 0x" << std::hex << std::uppercase << messageId
    << std::dec << std::nouppercase << " (" << messageId << ")\n";

  switch (convert) {
    case cvt_nt:
      messageId = reinterpret_cast<ULONG (*)(NTSTATUS)>(
          GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError")
        )(messageId);
      std::cout << "Converted from NTSTATUS = 0x" << std::hex << std::uppercase
        << messageId << std::dec << std::nouppercase << " (" << messageId << ")\n";
      break;
    default:
      break;
  }

  char buffer[1024];

  auto len = FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr, messageId, 0, buffer, 1024, nullptr
  );

  if (!len) {
    std::cout << "Couldn't decode result code.\n";
    return GetLastError();
  }

  std::cout << buffer;

  return 0;
}
