#include <Windows.h>
#include <stdio.h>

const int CUSTOM_MODE = 0x07000000;

int main(int argc, char *argv[]) {
  int setin = 0;
  int setout = 0;
  DWORD pid = 0;

  for (int i = 1; i < argc; i++) {
    auto arg = argv[i];
    if (arg[0] == '-' && arg[1] == 'h' && arg[2] == 0) {
      printf("Usage: isvt.exe (-p pid) [ioa][+, -, =xxxx]\n\n"
             "Selectors: i = stdin\n"
             "           o = stdout\n"
             "           a = all\n"
             "Mode:      + = enable vt mode\n"
             "           - = disable vt mode\n"
             "           = = set a custom mode in hex\n");
      return 0;
    }

    if (arg[0] == '-' && arg[1] == 'p' && arg[2] == 0) {
      if (i + 1 >= argc) {
        printf("need pid\n");
        return 1;
      }
      int j = 0;
      while (argv[i+1][j] >= '0' && argv[i+1][j] <= '9') {
        pid = pid * 10 + (argv[i+1][j] - '0');
        j++;
      }
      if (j == 0) {
        printf("%s is not a pid\n", argv[i+1]);
        return 1;
      }
      i++;
      continue;
    }

    if (arg[0] == 0 || arg[1] == 0 || (arg[1] != '=' && arg[2] != 0)) {
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
    } else if (arg[1] == '=') {
      int mode = 0;
      int j = 2;
      if (arg[2] == '0' && (arg[3] == 'x' || arg[3] == 'X')) {
        j = 4;
      }
      while (true) {
        int d;
        if (arg[j] >= '0' && arg[j] <= '9') {
          d = arg[j] - '0';
        } else if (arg[j] >= 'a' && arg[j] <= 'f') {
          d = arg[j] - 'a' + 0xA;
        } else if (arg[j] >= 'A' && arg[j] <= 'F') {
          d = arg[j] - 'A' + 0xA;
        } else if (arg[j] == 0) {
          break;
        } else {
          printf("bad hex: %s\n", arg);
          return 1;
        }

        mode = mode * 0x10 + d;
        j++;
      }
      if (arg[0] == 'i') {
        setin = mode | CUSTOM_MODE;
      } else if (arg[0] == 'o') {
        setout = mode | CUSTOM_MODE;
      } else if (arg[0] == 'a') {
        setin = setout = mode | CUSTOM_MODE;
      } else {
        printf("what does '%c' mean?\n", arg[0]);
        return 1;
      }
    }
  }

  if (pid) {
    if (!FreeConsole()) {
      printf("FreeConsole failed\n");
      return 1;
    }
    if (!AttachConsole(pid)) {
      printf("AttachConsole failed\n");
      return 1;
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
  } else if (setin & CUSTOM_MODE) {
    SetConsoleMode(in, setin & ~CUSTOM_MODE);
  }
  if (setout == 1) {
    SetConsoleMode(out, outmode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  } else if (setout == -1) {
    SetConsoleMode(out, outmode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  } else if (setout & CUSTOM_MODE) {
    SetConsoleMode(out, outmode & ~CUSTOM_MODE);
  }

  if (setin | setout) {
    static const char *things[] = { "off", "", "on" };
    if (setin == -1 || setin == 1) {
      printf("vt[in] = %s\n", things[setin+1]);
    } else if (setin & CUSTOM_MODE) {
      printf("mode[in] = 0x%X\n", setin & ~CUSTOM_MODE);
    }
    if (setout == -1 || setout == 1) {
      printf("vt[out] = %s\n", things[setout+1]);
    } else if (setout & CUSTOM_MODE) {
      printf("mode[out] = 0x%X\n", setout & ~CUSTOM_MODE);
    }
  }
  return 0;
}
