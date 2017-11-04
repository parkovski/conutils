#include <Windows.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  int setin = 0;
  int setout = 0;

  for (int i = 1; i < argc; i++) {
    auto arg = argv[i];
    if (arg[0] == '-' && arg[1] == 'h' && arg[2] == 0) {
      printf("Usage: isvt.exe [ioa][+-]\n\n"
             "Selectors: i = stdin\n"
             "           o = stdout\n"
             "           a = all\n"
             "Mode:      + = enable vt mode\n"
             "           - = disable vt mode\n");
      return 0;
    }

    if (arg[0] == 0 || arg[1] == 0 || arg[2] != 0) {
      printf("ignoring unknown option %s\n", arg);
      continue;
    }

    if (arg[1] == '+') {
      if (arg[0] == 'i') setin = 1;
      else if (arg[0] == 'o') setout = 1;
      else if (arg[0] == 'a') setin = setout = 1;
    } else if (arg[1] == '-') {
      if (arg[0] == 'i') setin = -1;
      else if (arg[0] == 'o') setout = -1;
      else if (arg[0] == 'a') setin = setout = -1;
    }
  }

  auto in = GetStdHandle(STD_INPUT_HANDLE);
  auto out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD inmode, outmode;
  GetConsoleMode(in, &inmode);
  GetConsoleMode(out, &outmode);
  bool vt = inmode & ENABLE_VIRTUAL_TERMINAL_INPUT;
  bool outvt = inmode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  printf("in: 0x%X; vt=%s\n", inmode, vt ? "on" : "off");
  printf("out: 0x%X; vt=%s\n", inmode, outvt ? "on" : "off");
  if (setin == 1) {
    SetConsoleMode(in, inmode | ENABLE_VIRTUAL_TERMINAL_INPUT);
  } else if (setin == -1) {
    SetConsoleMode(in, inmode & ~ENABLE_VIRTUAL_TERMINAL_INPUT);
  }
  if (setout == 1) {
    SetConsoleMode(out, inmode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  } else if (setout == -1) {
    SetConsoleMode(out, inmode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }

  if (setin | setout) {
    static const char *things[] = { "off", "", "on" };
    if (setin == setout) {
      printf("vt[in, out] = %s\n", things[setin+1]);
    } else {
      if (setin) {
        printf("vt[in] = %s\n", things[setin+1]);
      }
      if (setout) {
        printf("vt[out] = %s\n", things[setout+1]);
      }
    }
  }
  return 0;
}
