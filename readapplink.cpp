#include <Windows.h>
#include <WinNT.h>
#include <fileapi.h>
#include <iostream>

typedef struct _REPARSE_APPEXECLINK_READ_BUFFER { // For tag IO_REPARSE_TAG_APPEXECLINK
  DWORD  ReparseTag;
  WORD   ReparseDataLength;
  WORD   Reserved;
  ULONG  Version;    // Currently version 3
  // Start of data of length ReparseDataLength.
  WCHAR  StringList[1];  // Multistring (Consecutive UTF-16 strings each ending with a NUL)
  /* There are normally 4 strings here. Ex:
    Package ID:  L"Microsoft.WindowsTerminal_8wekyb3d8bbwe"
    Entry Point: L"Microsoft.WindowsTerminal_8wekyb3d8bbwe!App"
    Executable:  L"C:\Program Files\WindowsApps\Microsoft.WindowsTerminal_1.4.3243.0_x64__8wekyb3d8bbwe\wt.exe"
    Applic. Type: L"0"   // Integer as ASCII. "0" = Desktop bridge application; Else sandboxed UWP application
  */
} APPEXECLINK_READ_BUFFER, *PAPPEXECLINK_READ_BUFFER;

union buffer {
  APPEXECLINK_READ_BUFFER ael;
  char ch[4096];
};

void printstrings(wchar_t *stringlist, size_t datasize) {
  size_t size =
    sizeof(APPEXECLINK_READ_BUFFER)
    - offsetof(APPEXECLINK_READ_BUFFER, StringList);
  while (size < datasize) {
    std::wcout << L"> " << stringlist << L"\n";
    do {
      ++stringlist;
      size += sizeof(wchar_t);
    } while (*stringlist);
    ++stringlist;
    size += sizeof(wchar_t);
  }
  std::wcout << L"Read " << size << " bytes\n";
}

int wmain(int argc, wchar_t *argv[]) {
  if (argc == 1) {
    std::wcout << L"Need filename\n";
    return 1;
  }

  auto file = CreateFileW(
    argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr
  );
  buffer buffer;
  DWORD bytes;
  DeviceIoControl(
    file, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.ch, sizeof buffer.ch,
    &bytes, nullptr
  );

  CloseHandle(file);

  if (buffer.ael.ReparseTag != IO_REPARSE_TAG_APPEXECLINK) {
    if (buffer.ael.ReparseTag == IO_REPARSE_TAG_SYMLINK) {
      std::wcout << L"File is a symbolic link\n";
      return 1;
    } else {
      std::wcout << L"Not an appexeclink\n";
      std::wcout << L"ReparseTag = 0x" << std::hex << std::uppercase
        << buffer.ael.ReparseTag << std::dec << std::nouppercase << L"\n";
      return 1;
    }
  }

  std::wcout << L"ReparseDataLength = " << buffer.ael.ReparseDataLength
    << L"\nReserved = " << buffer.ael.Reserved
    << L"\nVersion = " << buffer.ael.Version;

  std::wcout << L"\nStrings:\n";
  printstrings(buffer.ael.StringList, buffer.ael.ReparseDataLength);
}
