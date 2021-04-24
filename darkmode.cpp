#include <Windows.h>
#include <cstdio>

#pragma comment(lib, "uxtheme.lib")

int main() {
  auto uxtheme = LoadLibraryW(L"uxtheme.dll");
  if (!uxtheme) {
    return 1;
  }

  auto pShouldSystemUseDarkMode = reinterpret_cast<bool (WINAPI *)()>(
    GetProcAddress(uxtheme, MAKEINTRESOURCEA(138))
  );
  if (!pShouldSystemUseDarkMode) {
    FreeLibrary(uxtheme);
    return 2;
  }

  if (pShouldSystemUseDarkMode()) {
    puts("dark");
  } else {
    puts("light");
  }

  FreeLibrary(uxtheme);
  return 0;
}
