#include <cwchar>
#include <exception>
#define UNICODE
#define _UNICODE
#include <Windows.h>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>

#ifndef NDEBUG
#define DEBUG(x) std::wcout << x << L"\n"
#else
#define DEBUG(x)
#endif

std::wstring getCurDir() {
  std::wstring dir;
  auto length = GetCurrentDirectory(0, nullptr);
  dir.resize(length);
  GetCurrentDirectory(length, dir.data());
  dir.erase(dir.end() - 1);
  return dir;
}

void goUp(std::wstring &path) {
  while (path.ends_with(L'\\') || path.ends_with(L'/')) {
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

struct Matcher {
  std::wstring baseDir;
  std::vector<std::wstring> globParts;
  std::vector<std::vector<std::wstring>> dirGlobs;
  std::vector<std::wstring> searchDirs;
  std::vector<std::wstring> files;

  Matcher(std::wstring baseDir)
    : baseDir{std::move(baseDir)}
  {}

  std::vector<std::wstring> &getGlob(int n) {
    if (n == dirGlobs.size()) {
      return globParts;
    }
    return dirGlobs[n];
  }

  void parse(std::wstring_view glob) {
    size_t i = 0, j = 0;
    for (; j < glob.length(); ++j) {
      switch (glob[j]) {
      case L'*':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        if (j + 1 < glob.length() && glob[j + 1] == '*') {
          if (globParts.empty() && (
                j + 2 == glob.length()
                || glob[j + 2] == L'/' || glob[j + 2] == L'\\'
              )) {
            globParts.emplace_back(L"**");
          } else {
            throw std::exception{"** must be alone in a path segment"};
          }
          ++j;
        } else {
          globParts.emplace_back(L"*");
        }
        i = j + 1;
        break;

      case L'?':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        globParts.emplace_back(glob.substr(j, 1));
        i = j + 1;
        break;

      case '[':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        i = j;
        while (++j < glob.length() && glob[j] != ']') {
        }
        if (j == glob.length()) {
          throw std::exception{"missing ']'"};
        } else if (j == i + 1) {
          throw std::exception("empty '[]' not allowed");
        }
        globParts.emplace_back(glob.substr(i, j - i + 1));
        i = j + 1;
        break;

      case '{':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        i = j;
        while (++j < glob.length() && glob[j] != '}') {
        }
        if (j == glob.length()) {
          throw std::exception{"missing '}'"};
        }
        DEBUG("parsed " << glob.substr(i, j - i + 1));
        globParts.emplace_back(glob.substr(i, j - i + 1));
        i = j + 1;
        break;

      case '`':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        if (j + 1 < glob.length()
            && (glob[j + 1] == '*' || glob[j + 1] == '?'
              || glob[j + 1] == '[' || glob[j + 1] == ']'
              || glob[j + 1] == '{' || glob[j + 1] == '}'
              || glob[i + 1] == '`')) {
          globParts.emplace_back(glob.substr(j, 2));
          i = ++j + 1;
        }

      case '/':
      case '\\':
        if (i != j) {
          globParts.emplace_back(glob.substr(i, j - i));
        }
        if (globParts.empty()) {
          i = j + 1;
          continue;
        }
        dirGlobs.emplace_back(std::move(globParts));
        globParts = {};
        i = j + 1;
        break;
      }
    }
    if (i < j) {
      globParts.emplace_back(glob.substr(i));
    }
  }

  bool findRangeBetweenStars(
    std::wstring &filename, const std::vector<std::wstring> &glob,
    size_t gi, size_t gi2, size_t &fi
  ) {
    size_t gi0 = gi;
    size_t fi0 = fi;

    while (fi0 < filename.length()) {
      for (; gi < gi2; ++gi) {
        switch (glob[gi][0]) {
        case L'?':
          if (fi == filename.length()) {
            return false;
          }
          ++fi;
          break;

        case L'[':
          for (size_t pi = 1; pi < glob[gi].length() - 1; ++pi) {
            wchar_t c1[2] = { glob[gi][pi], 0 };
            wchar_t c2[2] = { filename[fi], 0 };
            if (CompareString(
              LOCALE_INVARIANT, NORM_IGNORECASE, c1, -1, c2, -1
            ) == CSTR_EQUAL) {
              goto foundChar;
            }
          }
          goto tryAgain;

        foundChar:
          fi0 = fi;
          ++fi;
          break;

        case L'{':
          {
            size_t pi0 = 1, pi = 1;
            while (true) {
              if (pi == glob[gi].length()) {
                goto tryAgain;
              }
              for (; pi < glob[gi].length(); ++pi) {
                if (glob[gi][pi] == L',' || glob[gi][pi] == L'}') {
                  break;
                }
              }
              if (pi == pi0) {
                break;
              }
              if (filename.length() - fi >= pi - pi0) {
                auto s =
                  std::wstring_view{filename}.substr(fi, pi - pi0);
                if (CompareString(
                  LOCALE_INVARIANT, NORM_IGNORECASE,
                  glob[gi].c_str() + pi0, pi - pi0, s.data(), s.length()
                ) == CSTR_EQUAL) {
                  DEBUG(L"In *{}: found match: " << filename);
                  fi += pi - pi0;
                  fi0 = fi - 1;
                  break;
                }
              }
              pi0 = ++pi;
            }
          }
          break;

        case '`':
          if (glob[gi].length() > 1
              && (glob[gi][1] == '*' || glob[gi][1] == '?'
                || glob[gi][1] == '[' || glob[gi][1] == ']'
                || glob[gi][1] == '{' || glob[gi][1] == '}'
                || glob[gi][1] == '`')) {
            if (filename[fi] == glob[gi][1]) {
              fi0 = fi + 1;
              fi += 2;
              break;
            } else {
              goto tryAgain;
            }
          }
          [[fallthrough]];

        default:
          {
            auto s =
              std::wstring_view{filename}.substr(fi, glob[gi].length());
            if (CompareString(
              LOCALE_INVARIANT, NORM_IGNORECASE,
              glob[gi].c_str(), -1, s.data(), s.length()
            ) == CSTR_EQUAL) {
              DEBUG(L"found match: " << filename);
              fi += glob[gi].length();
              fi0 = fi - 1;
              break;
            }
          }
          goto tryAgain;
        }
      }
      return true;

    tryAgain:
      fi = ++fi0;
      gi = gi0;
    }

    return false;
  }

  bool test(std::wstring filename, int dirnr) {
    const std::vector<std::wstring> &glob = getGlob(dirnr);
    size_t gi = 0, fi = 0;
    for (; gi < glob.size(); ++gi) {
      auto const &part = glob[gi];
      switch (part[0]) {
      case L'?':
        if (fi < filename.length()) {
          ++fi;
        } else {
          return false;
        }
        break;

      case L'*':
        {
          if (gi == glob.size() - 1) {
            return true;
          }
          size_t gi2 = gi + 1;
          for (; gi2 < glob.size(); ++gi2) {
            if (glob[gi2] == L"*") {
              break;
            }
          }
          size_t fi2 = fi;
          if (findRangeBetweenStars(filename, glob, gi + 1, gi2, fi2)) {
            gi = gi2 - 1;
            fi = fi2;
          } else {
            return false;
          }
        }
        break;

      case L'[':
        for (size_t pi = 1; pi < glob[gi].length() - 1; ++pi) {
          wchar_t c1[2] = { glob[gi][pi], 0 };
          wchar_t c2[2] = { filename[fi], 0 };
          if (CompareString(
            LOCALE_INVARIANT, NORM_IGNORECASE, c1, -1, c2, -1
          ) == CSTR_EQUAL) {
            goto foundChar;
          }
        }
        return false;

      foundChar:
        break;

      case L'{':
        {
          size_t pi0 = 1, pi = 1;
          while (true) {
            if (pi0 == glob[gi].length()) {
              return false;
            }
            for (; pi < glob[gi].length(); ++pi) {
              if (glob[gi][pi] == L',' || glob[gi][pi] == L'}') {
                break;
              }
            }
            if (pi == pi0) {
              break;
            }
            if (filename.length() - fi >= pi - pi0) {
              auto s =
                std::wstring_view{filename}.substr(fi, pi - pi0);
              if (CompareString(
                LOCALE_INVARIANT, NORM_IGNORECASE,
                glob[gi].c_str() + pi0, pi - pi0, s.data(), s.length()
              ) == CSTR_EQUAL) {
                DEBUG(L"found match: " << filename);
                fi += pi - pi0;
                break;
              }
            }
            pi0 = ++pi;
          }
        }
        break;

      case '`':
        if (glob[gi].length() > 1
            && (glob[gi][1] == '*' || glob[gi][1] == '?'
              || glob[gi][1] == '[' || glob[gi][1] == ']'
              || glob[gi][1] == '{' || glob[gi][1] == '}'
              || glob[gi][1] == '`')) {
          if (filename[fi] == glob[gi][1]) {
            fi += 2;
            break;
          } else {
            return false;
          }
        }
        [[fallthrough]];

      default:
        {
          auto s =
            std::wstring_view{filename}.substr(fi, glob[gi].length());
          if (CompareString(
            LOCALE_INVARIANT, NORM_IGNORECASE,
            glob[gi].c_str(), -1, s.data(), s.length()
          ) == CSTR_EQUAL) {
            fi += glob[gi].length();
            break;
          } else {
            return false;
          }
        }
      }
    }

    return fi == filename.length();
  }

  struct MatchResult {
    std::vector<std::wstring> matches;
    std::vector<std::wstring> printMatches;
  };

  MatchResult match(std::wstring &baseDir,
                    std::wstring &baseDirPrint,
                    int &dirnr) {
    if (dirnr > dirGlobs.size()) {
      return {};
    }

    MatchResult result;
    WIN32_FIND_DATA findData;
    if (!baseDir.empty()
        && !baseDir.ends_with(L'\\') && !baseDir.ends_with('/')) {
      baseDir.push_back(L'\\');
    }
    if (!baseDirPrint.empty()
        && !baseDirPrint.ends_with(L'\\') && !baseDirPrint.ends_with('/')) {
      baseDirPrint.push_back(L'\\');
    }

    DEBUG(L"baseDir = " << baseDir << L", baseDirPrint = " << baseDirPrint);

    auto addPaths = [&](std::wstring filename, std::wstring filenamePrint){
      DEBUG(L"added " << filename << L" (" << filenamePrint << L")");
      result.matches.emplace_back(std::move(filename));
      result.printMatches.emplace_back(std::move(filenamePrint));
    };

    auto const &glob = getGlob(dirnr);
    if (glob.size() == 1 && glob[0] == L".") {
      baseDirPrint.append(L".\\");
      return match(baseDir, baseDirPrint, ++dirnr);
    } else if (glob.size() == 1 && glob[0] == L"..") {
      goUp(baseDir);
      baseDirPrint.append(L"..\\");
      return match(baseDir, baseDirPrint, ++dirnr);
    }

    DEBUG(L"new baseDir = " << baseDir);

    baseDir.push_back('*');
    HANDLE hFind = FindFirstFile(baseDir.c_str(), &findData);
    baseDir.erase(baseDir.end() - 1);
    if (hFind == INVALID_HANDLE_VALUE) {
      throw std::exception{"FindFirstFile failed."};
    }
    do {
      if (!wcscmp(findData.cFileName, L".")
          || !wcscmp(findData.cFileName, L"..")) {
        continue;
      }
      if (test(findData.cFileName, dirnr)) {
        std::wstring filename{baseDir};
        std::wstring filenamePrint{baseDirPrint};
        filename.append(findData.cFileName);
        filenamePrint.append(findData.cFileName);
        addPaths(std::move(filename), std::move(filenamePrint));
      }
    } while (FindNextFile(hFind, &findData));

    CloseHandle(hFind);

    return result;
  }

  MatchResult operator()(std::wstring &baseDir,
                         std::wstring &baseDirPrint,
                         int dirnr = 0) {
    auto matchResult = match(baseDir, baseDirPrint, dirnr);
    MatchResult result;
    for (int i = 0; i < matchResult.matches.size(); ++i) {
      auto attr = GetFileAttributes(matchResult.matches[i].c_str());
      if (dirnr == dirGlobs.size()) {
        result.matches
          .emplace_back(std::move(*(matchResult.matches.data() + i)));
        result.printMatches
          .emplace_back(std::move(*(matchResult.printMatches.data() + i)));
      } else if (attr & FILE_ATTRIBUTE_DIRECTORY && dirnr < dirGlobs.size()) {
        auto r = (*this)(
          matchResult.matches[i],
          matchResult.printMatches[i],
          dirnr + 1
        );
        for (int j = 0; j < r.matches.size(); ++j) {
          result.matches.emplace_back(std::move(*(r.matches.data() + j)));
          result.printMatches.emplace_back(std::move(*(r.printMatches.data() + j)));
        }
      } else {
        DEBUG(L"Should not be reached!");
        __assume(0);
      }
    }

    return result;
  }
};

int wmain(int argc, wchar_t *argv[]) {
  std::wstring baseDir, printDir, absolute;
  if (argc == 2) {
    baseDir = getCurDir();
  } else if (argc == 3) {
    baseDir = argv[2];
    printDir = baseDir;
  } else {
    std::cerr << "usage: glob <glob> [<directory>]\n";
    return 1;
  }

  if (baseDir.length() >= 2 && (baseDir[0] | 0x20) >= L'a' && (baseDir[0] | 0x20) <= L'z'
      && baseDir[1] == L':') {
    //
  } else if (baseDir[0] == L'\\' || baseDir[0] == L'/') {
    auto curdir = getCurDir();
    baseDir = curdir.substr(0, 2) + baseDir;
  } else {
    std::wstring tmp = getCurDir();
    tmp.push_back(L'\\');
    tmp.append(baseDir);
    baseDir = std::move(tmp);
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
  for (; j < baseDir.length(); ++j) {
    if (baseDir[j] == '\\' || baseDir[j] == '/') {
      if (i == j) {
        i = j + 1;
        continue;
      }
      auto segment = std::wstring_view{&baseDir[i], j - i};
      do_segment(segment, absolute);
      i = j + 1;
    }
  }
  if (j > i) {
    do_segment(std::wstring_view{&baseDir[i], j - i}, absolute);
  }

  if (!printDir.empty() &&
      !printDir.ends_with(L'\\') && !printDir.ends_with(L'/')) {
    printDir.push_back(L'\\');
  }

  Matcher matcher{std::move(absolute)};
  try {
    matcher.parse(argv[1]);
    auto r = matcher(baseDir, printDir);
    DEBUG(L"\n\n");
    for (int i = 0; i < r.printMatches.size(); ++i) {
      std::wcout << r.printMatches[i] << L"\n";
    }
  } catch (std::exception &e) {
    std::cout << e.what() << "\n";
    return 1;
  }

  return 0;
}
