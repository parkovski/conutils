#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>

//#define DEBUG(x) std::wcout << x << L"\n";
#define DEBUG(x)

std::wstring getCurDir() {
  std::wstring dir;
  auto length = GetCurrentDirectory(0, nullptr);
  dir.resize(length);
  GetCurrentDirectory(length, dir.data());
  dir.erase(dir.end() - 1);
  return dir;
}

void goUp(std::wstring &path) {
  if (path.ends_with(L'\\') || path.ends_with(L'/')) {
    path.erase(path.end() - 1);
  }
  auto slash = path.find_last_of(L'/');
  auto backslash = path.find_last_of(L'\\');
  if (slash == std::wstring::npos) {
    if (backslash == std::wstring::npos) {
      return;
    } else {
      path.erase(backslash + 1);
    }
  } else if (backslash == std::wstring::npos) {
    path.erase(slash + 1);
  }
}

void collect(std::wstring_view glob, std::vector<std::wstring> &globParts) {
  DEBUG(glob);
  size_t i = 0, j = 0;
  for (; j < glob.length(); ++j) {
    switch (glob[j]) {
    case L'?':
    case L'*':
      if (i != j) {
        globParts.emplace_back(glob.substr(i, j - i));
      }
      globParts.emplace_back(glob.substr(j, 1));
      i = j + 1;
      break;

    default:
      break;
    }
  }
  if (i < j) {
    globParts.emplace_back(glob.substr(i));
  }
}

bool match(std::wstring_view file, const std::vector<std::wstring> globParts) {
  size_t gi = 0, fi = 0;
  for (; gi < globParts.size(); ++gi) {
    auto const &part = globParts[gi];
    if (part.length() == 1) {
      switch(part[0]) {
      case '?':
        if (fi < file.length()) {
          ++fi;
        } else {
          return false;
        }
        break;

      case '*':
        {
          if (gi == globParts.size() - 1) {
            return true;
          }
          auto const &nextPart = globParts[gi + 1];
          auto i = file.find(nextPart.data(), fi, nextPart.length());
          if (i == std::wstring::npos) {
            return false;
          }
          fi = i + nextPart.length();
          ++gi;
        }
        break;

      default:
        if (file[fi] != part[0]) {
          return false;
        }
        ++fi;
        break;
      }
    } else {
      if (file.substr(fi, part.length()) != part) {
        return false;
      }
      fi += part.length();
    }
  }
  return fi == file.length();
}

int wmain(int argc, wchar_t *argv[]) {
  std::wstring dir, absolute;
  if (argc == 2) {
    dir = getCurDir();
  } else if (argc == 3) {
    dir = argv[2];
  } else {
    std::cerr << "usage: glob <glob> [<directory>]\n";
    return 1;
  }

  if (dir.length() >= 2 && (dir[0] | 0x20) >= L'a' && (dir[0] | 0x20) <= L'z'
      && dir[1] == L':') {
    //
  } else if (dir[0] == L'\\' || dir[0] == L'/') {
    auto curdir = getCurDir();
    dir = curdir.substr(0, 2) + dir;
  } else {
    std::wstring tmp = getCurDir();
    tmp.push_back(L'\\');
    tmp.append(dir);
    dir = std::move(tmp);
  }

  auto do_segment = [](std::wstring_view segment, std::wstring &absolute) {
    if (segment == L".") {
    } else if (segment == L"..") {
      goUp(absolute);
    } else {
      absolute.append(segment);
      absolute.push_back(L'\\');
    }
  };

  size_t i = 0, j = 0;
  for (; j < dir.length(); ++j) {
    if (dir[j] == '\\' || dir[j] == '/') {
      if (i == j) {
        i = j + 1;
        continue;
      }
      auto segment = std::wstring_view{&dir[i], j - i};
      do_segment(segment, absolute);
      i = j + 1;
    }
  }
  if (j > i) {
    do_segment(std::wstring_view{&dir[i], j - i}, absolute);
  }

  DEBUG(L"search path: " << absolute);

  std::vector<std::wstring> dirFiles;
  std::vector<std::wstring> matchFiles;

  WIN32_FIND_DATA findData;
  HANDLE hFind;

  absolute.push_back(L'*');
  hFind = FindFirstFile(absolute.c_str(), &findData);
  if (hFind == INVALID_HANDLE_VALUE) {
    return 1;
  }
  do {
    if (!wcscmp(findData.cFileName, L".") ||
        !wcscmp(findData.cFileName, L"..")) {
      continue;
    }
    dirFiles.emplace_back(findData.cFileName);
  } while (FindNextFile(hFind, &findData) != 0);

  std::wstring_view glob{argv[1]};
  std::vector<std::wstring> globParts;
  collect(glob, globParts);
  for (auto const &part : globParts) {
    DEBUG(part);
  }

  for (auto const &file : dirFiles) {
    if (match(file, globParts)) {
      matchFiles.emplace_back(file);
    }
  }

  for (auto const &file : matchFiles) {
    std::wcout << file << L"\n";
  }
}
