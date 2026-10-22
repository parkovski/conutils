#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <string_view>
#include <iostream>

void printHelp() {
  std::cout
    << "Usage: touch [Options...] File...\n"
    << "Options:\n"
    << "  -a                   Update access time only.\n"
    << "  -c, --no-create      Don't create a new file.\n"
    << "  -m                   Update modified (write) time only.\n"
    << "  -h, --no-dereference Open reparse points instead of following them.\n";
}

int wmain(int argc, wchar_t *argv[]) {
  int i = 1;
  DWORD createFlags = OPEN_ALWAYS;
  bool updateAccessTime = true;
  bool updateModifiedTime = true;
  DWORD fileFlags = FILE_ATTRIBUTE_NORMAL;
  for (; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    if (arg == L"-a") {
      updateModifiedTime = false;
    } else if (arg == L"-m") {
      updateAccessTime = false;
    } else if (arg == L"-c" || arg == L"--no-create") {
      createFlags = OPEN_EXISTING;
    } else if (arg == L"-h" || arg == L"--no-dereference") {
      fileFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
    } else if (arg == L"--help") {
      printHelp();
      return 0;
    } else if (arg == L"--") {
      ++i;
      break;
    } else if (arg[0] == L'-') {
      std::wcerr << L"Unrecognized option: " << arg << L"\n";
      return 1;
    } else {
      break;
    }
  }

  if (i == argc) {
    std::cerr << "Error: No files specified.\n";
    return 1;
  }

  int mainReturn = 0;
  for (; i < argc; ++i) {
    HANDLE hFile = CreateFile(argv[i], GENERIC_WRITE, 0, nullptr,
                              createFlags, fileFlags, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
      mainReturn = 1;
      auto err = GetLastError();
      if (err == ERROR_ACCESS_DENIED) {
        std::wcerr << L"Access denied: " << argv[i] << L"\n";
      } else if (err == ERROR_FILE_NOT_FOUND) {
        std::wcerr << L"File not found: " << argv[i] << L"\n";
      } else {
        std::wcerr << L"Error opening " << argv[i] << L": " << err << L"\n";
      }
    } else {
      FILETIME now;
      SYSTEMTIME st;
      GetSystemTime(&st);
      SystemTimeToFileTime(&st, &now);
      FILETIME *accessTime = updateAccessTime ? &now : nullptr;
      FILETIME *modifiedTime = updateModifiedTime ? &now : nullptr;
      if (!SetFileTime(hFile, nullptr, accessTime, modifiedTime)) {
        std::wcerr << L"SetFileTime failed for " << argv[i] << L": "
          << GetLastError() << L"\n";
      }
      CloseHandle(hFile);
    }
  }

  return mainReturn;
}
