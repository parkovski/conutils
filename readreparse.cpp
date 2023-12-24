#include <Windows.h>
#include <WinNT.h>
#include <fileapi.h>
#include <iostream>

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
    struct {
      ULONG Version;
      /* There are normally 4 strings here. Ex:
        Package ID:  L"Microsoft.WindowsTerminal_8wekyb3d8bbwe"
        Entry Point: L"Microsoft.WindowsTerminal_8wekyb3d8bbwe!App"
        Executable:  L"C:\Program Files\WindowsApps\Microsoft.WindowsTerminal_1.4.3243.0_x64__8wekyb3d8bbwe\wt.exe"
        Applic. Type: L"0"   // Integer as ASCII. "0" = Desktop bridge application; Else sandboxed UWP application
      */
      WCHAR StringList[1];
    } AppExecLinkReparseBuffer;
  } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

union buffer {
  REPARSE_DATA_BUFFER rd;
  char ch[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
};

void printstrings(wchar_t *stringlist, size_t datasize) {
  size_t size = sizeof(ULONG); // AppExecLinkReparseBuffer.Version
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

void printreparse(REPARSE_DATA_BUFFER *buffer) {
  std::wcout << L"ReparseTag = 0x" << std::hex << std::uppercase
    << buffer->ReparseTag << std::dec << std::nouppercase
    << L"\nReparseDataLength = " << buffer->ReparseDataLength
    << L"\nReserved = " << buffer->Reserved
    << L"\n";
}

void printmountpoint(REPARSE_DATA_BUFFER *buffer) {
  std::wcout
    << L"SubstituteNameOffset = " << buffer->MountPointReparseBuffer.SubstituteNameOffset
    << L"\nSubstituteNameLength = " << buffer->MountPointReparseBuffer.SubstituteNameLength
    << L"\nPrintNameOffset = " << buffer->MountPointReparseBuffer.PrintNameOffset
    << L"\nPrintNameLength = " << buffer->MountPointReparseBuffer.PrintNameLength
    << L"\nPathBuffer = " << buffer->MountPointReparseBuffer.PathBuffer
    << L"\n";
}

void printappexeclink(REPARSE_DATA_BUFFER *buffer) {
  std::wcout << L"Version = " << buffer->AppExecLinkReparseBuffer.Version
    << L"\n";
  printstrings(buffer->AppExecLinkReparseBuffer.StringList, buffer->ReparseDataLength);
}

void printsymboliclink(REPARSE_DATA_BUFFER *buffer) {
  std::wcout
    << L"SubstituteNameOffset = " << buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset
    << L"\nSubstituteNameLength = " << buffer->SymbolicLinkReparseBuffer.SubstituteNameLength
    << L"\nPrintNameOffset = " << buffer->SymbolicLinkReparseBuffer.PrintNameOffset
    << L"\nPrintNameLength = " << buffer->SymbolicLinkReparseBuffer.PrintNameLength
    << L"\nFlags = 0x" << std::hex << std::uppercase << buffer->SymbolicLinkReparseBuffer.Flags
    << std::dec << std::nouppercase
    << L"\nPathBuffer = " << buffer->SymbolicLinkReparseBuffer.PathBuffer
    << L"\n";
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
  if (file == INVALID_HANDLE_VALUE) {
    std::wcout << L"Open file failed\n";
    return 1;
  }
  buffer buffer;
  DWORD bytes;
  if (!DeviceIoControl(
    file, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.ch, sizeof buffer.ch,
    &bytes, nullptr
  )) {
    std::wcout << L"Read reparse point failed\n";
    return 1;
  }

  CloseHandle(file);

  printreparse(&buffer.rd);
  if (buffer.rd.ReparseTag == IO_REPARSE_TAG_SYMLINK) {
    std::wcout << L"Symbolic link:\n";
    printsymboliclink(&buffer.rd);
  } else if (buffer.rd.ReparseTag == IO_REPARSE_TAG_APPEXECLINK) {
    std::wcout << L"App exec link:\n";
    printappexeclink(&buffer.rd);
  } else if (buffer.rd.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
    std::wcout << L"Mount point:\n";
    printmountpoint(&buffer.rd);
  } else {
    std::wcout << L"Unknown reparse tag\n";
    return 1;
  }

  return 0;
}
