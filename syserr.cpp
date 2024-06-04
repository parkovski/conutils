#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
# include <winternl.h>
#else
# include <string.h>
#endif

#include <iostream>
#include <sstream>

int main(int argc, char *argv[]) {
  if (argc != 2 || (argv[1][0] < '0' || argv[1][0] > '9')) {
    std::cerr << "Usage: syserr <error-code>.\n";
    std::cerr << "Numbers can be in decimal or hex (0x...) format.\n";
    return 1;
  }

  std::stringstream ss;
#ifdef _WIN32
  DWORD messageId;
#else
  int messageId;
#endif
  if (argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X')) {
    ss << std::hex << &argv[1][2];
  } else {
    ss << argv[1];
  }
  ss >> messageId;

#ifdef _WIN32
  std::cout << "Code 0x" << std::hex << std::uppercase << messageId
    << std::dec << std::nouppercase << " (" << messageId << ")\n";
#else
  std::cout << "Code " << messageId << "\n";
#endif

#ifdef _WIN32
  auto fromNt = reinterpret_cast<ULONG (*)(NTSTATUS)>(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError")
  )(messageId);

  char buffer[1024];

  auto len = FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr, messageId, 0, buffer, 1024, nullptr
  );
  if (fromNt != ERROR_MR_MID_NOT_FOUND) {
    if (len) {
      std::cout << buffer;
      if (fromNt == messageId) {
        return 0;
      }
    }
    messageId = fromNt;
    std::cout << "Code from RtlNtStatusToDosError = 0x" << std::hex << std::uppercase
      << messageId << std::dec << std::nouppercase << " (" << messageId << ")\n";
    len = FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, messageId, 0, buffer, 1024, nullptr
    );
  }

  if (!len) {
    auto err = GetLastError();
    std::cout << "Not found.\n";
    return err;
  }

  std::cout << buffer;
#else
#if __linux__
  auto errname = strerrorname_np(messageId);
  if (errname) {
    std::cout << errname << ": " << strerror(messageId) << "\n";
  } else {
    std::cout << "Not found.\n";
  }
#else
  std::cout << strerror(messageId) << "\n";
#endif

  int altMessageId = -1;
  if (messageId > 0x7f) {
    altMessageId = -(int8_t)(messageId);
  }
  if (altMessageId != -1) {
#if __linux__
    auto alterrname = strerrorname_np(altMessageId);
    if (alterrname) {
      std::cout << "\nAlternate code (-errno): " << altMessageId << "\n"
                << alterrname << ": " << strerror(altMessageId) << "\n";
    }
#else
    std::cout << "\nAlternate code (-errno): " << altMessageId << "\n"
              << strerror(altMessageId) << "\n";
#endif
  }
#endif

  return 0;
}
