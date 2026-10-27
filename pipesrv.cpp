#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <string>
#include <string_view>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

const wchar_t *DefaultPipeName = L"\\\\.\\pipe\\pipesrv_default_pipe";

static void help() {
  std::wcout << L"Usage:\n"
    << L"  pipesrv <message>                send <message> to the pipe server on the default pipe name\n"
    << L"  pipesrv <pipe name> <message>    send message to the pipe server on <pipe name>\n"
    << L"  pipesrv -s [pipe name]           run a server on <pipe name>\n"
    << L"  pipesrv -w [pipe name]           wait for messages on <pipe name>\n";
}

static void runServer(const std::wstring &pipeName);
static void sendMessage(const std::wstring &pipeName,
                        const char *message);
static void waitForMessages(const std::wstring &pipeName);

int wmain(int argc, wchar_t *argv[]) {
  std::wstring pipeName = DefaultPipeName;
  std::wstring message;
  bool wait = false;
  if (argc == 2) {
    std::wstring_view arg{argv[1]};
    if (arg == L"-s") {
    } else if (arg == L"-w") {
      wait = true;
    } else {
      message = argv[1];
    }
  } else if (argc == 3) {
    std::wstring_view arg{argv[1]};
    if (arg == L"-s") {
      pipeName = argv[2];
    } else if (arg == L"-w") {
      pipeName = argv[2];
      wait = true;
    } else {
      pipeName = argv[1];
      message = argv[2];
    }
  } else {
    help();
    return 0;
  }

  if (!pipeName.starts_with(L"\\\\.\\pipe\\")) {
    pipeName = L"\\\\.\\pipe\\" + pipeName;
  }

  if (wait) {
    waitForMessages(pipeName);
  } else if (message.empty()) {
    runServer(pipeName);
  } else {
    int length = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, nullptr,
                                     0, nullptr, nullptr);
    char *m = new char[length];
    WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, m, length, nullptr,
                         nullptr);
    sendMessage(pipeName, m);
  }

  return 0;
}

const DWORD PipeBufferSize = 256;
static void runServer(const std::wstring &pipeName) {
  std::vector<HANDLE> clients;
  std::mutex mutex;
  bool exitFlag = false;

  while (true) {
    {
      std::unique_lock lock{mutex};
      if (exitFlag) {
        return;
      }
    }

    HANDLE pipe = CreateNamedPipe(
      pipeName.c_str(),
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
      PIPE_UNLIMITED_INSTANCES,
      PipeBufferSize,
      PipeBufferSize,
      0,
      nullptr
    );
    if (pipe == INVALID_HANDLE_VALUE) {
      std::wcerr << L"Error creating pipe!\n";
      return;
    }

    {
      std::unique_lock lock{mutex};
      if (exitFlag) {
        CloseHandle(pipe);
        return;
      }
    }

    if (!ConnectNamedPipe(pipe, nullptr)
        && GetLastError() != ERROR_PIPE_CONNECTED) {
      CloseHandle(pipe);
      continue;
    }

    {
      std::unique_lock lock{mutex};
      if (exitFlag) {
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return;
      }
    }

    std::thread{[pipe, &pipeName, &clients, &mutex, &exitFlag] () {
      auto closePipe = [&pipe, &clients] (std::unique_lock<std::mutex> &lock) {
        (void)lock;
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        auto it = std::find(clients.begin(), clients.end(), pipe);
        if (it == clients.end()) {
          return;
        }
        clients.erase(it);
      };

      char message[PipeBufferSize];
      bool addedToClients = false;
      while (true) {
        {
          std::unique_lock lock{mutex};
          if (exitFlag) {
            return;
          }
        }

        DWORD bytesRead;
        if (!ReadFile(pipe, message, sizeof(message), &bytesRead, nullptr)) {
          if (GetLastError() == ERROR_BROKEN_PIPE) {
            std::unique_lock lock{mutex};
            closePipe(lock);
            return;
          } else {
            std::wcerr << L"ReadFile failed: 0x" << std::hex << GetLastError()
              << std::dec << L".\n";
            std::unique_lock lock{mutex};
            closePipe(lock);
            return;
          }
        }
        std::cout << "Received message: " << message << ".\n";

        if (std::string_view{message} == "wait") {
          if (!addedToClients) {
            std::unique_lock lock{mutex};
            clients.emplace_back(pipe);
            addedToClients = true;
            return;
          }
        } else if (std::string_view{message} == "exit") {
          std::unique_lock lock{mutex};
          if (exitFlag) {
            closePipe(lock);
            return;
          }
          exitFlag = true;
          DWORD bytes;
          WriteFile(pipe, "exit", 5, &bytes, nullptr);
          for (auto client : clients) {
            if (pipe == client) {
              continue;
            }
            WriteFile(client, "exit", 5, &bytes, nullptr);
            DisconnectNamedPipe(client);
            CloseHandle(client);
          }
          clients.clear();

          closePipe(lock);
          lock.unlock();

          HANDLE newPipe = CreateFile(pipeName.c_str(), GENERIC_WRITE, 0,
                                      nullptr, OPEN_EXISTING, 0, nullptr);
          if (newPipe == INVALID_HANDLE_VALUE) {
            return;
          }
          WriteFile(newPipe, "exit", 5, &bytes, nullptr);
          CloseHandle(newPipe);
          return;
        } else {
          std::unique_lock lock{mutex};
          DWORD bytesWritten;
          WriteFile(pipe, "ok", 3, &bytesWritten, nullptr);
          for (int i = 0; i < clients.size(); ++i) {
            if (clients[i] == pipe) {
              continue;
            }
            if (!WriteFile(clients[i], message, bytesRead, &bytesWritten,
                           nullptr)) {
              std::wcerr << L"WriteFile failed; erasing client.\n";
              clients.erase(clients.begin() + i);
              --i;
            }
          }
        }
      }
    }}.detach();
  }
}

const DWORD WaitTime = 1000;
static void sendMessage(const std::wstring &pipeName,
                        const char *message) {
  bool waited = false;
  HANDLE pipe;
  while (true) {
    pipe = CreateFile(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                      nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_PIPE_BUSY) {
        if (waited) {
          std::wcerr << L"Pipe took too long to respond!\n";
          return;
        }
        WaitNamedPipe(pipeName.c_str(), WaitTime);
        waited = true;
        continue;
      }
      std::wcerr << L"Couldn't connect to pipe!\n";
      return;
    }
    break;
  }

  DWORD pipeMode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr)) {
    CloseHandle(pipe);
    std::wcerr << L"SetNamedPipeHandleState failed!\n";
    return;
  }

  DWORD bytesWritten;
  if (!WriteFile(pipe, message, strlen(message) + 1, &bytesWritten,
                 nullptr)) {
    CloseHandle(pipe);
    std::wcerr << L"Writing to pipe failed!\n";
    return;
  }

  DWORD bytesRead;
  char buffer[PipeBufferSize];
  if (!ReadFile(pipe, buffer, PipeBufferSize, &bytesRead, nullptr)) {
    CloseHandle(pipe);
    std::wcerr << L"Error reading response from server!\n";
    return;
  }
  CloseHandle(pipe);
}

static void waitForMessages(const std::wstring &pipeName) {
  bool waited = false;
  HANDLE pipe;
  while (true) {
    pipe = CreateFile(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                      nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_PIPE_BUSY) {
        if (waited) {
          std::wcerr << L"Pipe took too long to respond!\n";
          return;
        }
        WaitNamedPipe(pipeName.c_str(), WaitTime);
        waited = true;
        continue;
      }
      std::wcerr << L"Couldn't connect to pipe!\n";
      return;
    }
    break;
  }

  DWORD pipeMode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr)) {
    CloseHandle(pipe);
    std::wcerr << L"SetNamedPipeHandleState failed!\n";
    return;
  }

  DWORD bytes;
  if (!WriteFile(pipe, "wait", 5, &bytes, nullptr)) {
    CloseHandle(pipe);
    std::wcerr << L"Writing to pipe failed!\n";
    return;
  }

  char message[PipeBufferSize];
  while (true) {
    if (!ReadFile(pipe, message, PipeBufferSize, &bytes, nullptr)) {
      CloseHandle(pipe);
      std::wcerr << L"Reading from pipe failed!\n";
      return;
    }
    std::cout << "Received message: " << message << ".\n";
    if (std::string_view{message} == "exit") {
      CloseHandle(pipe);
      return;
    }
  }
}
