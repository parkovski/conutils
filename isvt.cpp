#include <Windows.h>
#include <stdio.h>
#include <string>

using std::string;

static const int CUSTOM_MODE = 0x05000000;
static const int CURRENT_MODE = 0x0A000000;

#ifndef ENABLE_AUTO_POSITION
#define ENABLE_AUTO_POSITION 0x100
#endif

DWORD parse_hex(string &s) {
  DWORD hex = 0;
  if (s.length() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s = s.substr(2);
  }

  for (char c : s) {
    int d;
    if (c >= '0' && c <= '9') {
      d = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      d = c - 'a' + 0xA;
    } else if (c >= 'A' && c <= 'F') {
      d = c - 'A' + 0xA;
    } else {
      return 0;
    }
    hex = (hex << 4) | d;
  }
  return hex;
}

#define CM(m) if (mode & m) { printf("%s%s", first ? "" : " | ", #m); first = false; }

void print_in(DWORD mode) {
  bool first = true;
  CM(ENABLE_PROCESSED_INPUT)
  CM(ENABLE_LINE_INPUT)
  CM(ENABLE_ECHO_INPUT)
  CM(ENABLE_WINDOW_INPUT)
  CM(ENABLE_MOUSE_INPUT)
  CM(ENABLE_INSERT_MODE)
  CM(ENABLE_QUICK_EDIT_MODE)
  CM(ENABLE_VIRTUAL_TERMINAL_INPUT)
  CM(ENABLE_AUTO_POSITION)
  printf("\n");
}

void print_out(DWORD mode) {
  bool first = true;
  CM(ENABLE_PROCESSED_OUTPUT)
  CM(ENABLE_WRAP_AT_EOL_OUTPUT)
  CM(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
  CM(DISABLE_NEWLINE_AUTO_RETURN)
  CM(ENABLE_LVB_GRID_WORLDWIDE)
  printf("\n");
}

#undef CM

#define CM(m) if (val == #m) { \
    if (add) mode |= invert ? ~m : m; \
    else mask &= invert ? ~m : m; \
    continue; \
  }

template<bool in>
bool set_mode(const char *str, DWORD &mode, DWORD &mask) {
  char c;
  bool add = true;
  bool invert = false;
  string val;
  mode = 0;
  mask = (DWORD)(-1);
  while ((c = *str++) != 0) {
    if (c == ' ' || c == '\t') {
      continue;
    }
    if (c == '|' || c == '+' || c == ',') {
      add = true;
      invert = false;
      continue;
    }
    if (c == '-') {
      add = false;
      invert = true;
      continue;
    }
    if (c == '&') {
      add = false;
      invert = false;
      continue;
    }
    if (c == '~') {
      invert = true;
      continue;
    }

    val.clear();
    --str;
    while (
      (c = *str)
      && (
             (c >= '0' && c <= '9')
          || (c >= 'a' && c <= 'z')
          || (c >= 'A' && c <= 'Z')
          || c == '_'
      )
    )
    {
      val.push_back(c);
      ++str;
    }

    if (val.length() == 0) {
      return false;
    } else {
      DWORD hex = parse_hex(val);
      if (hex == 0 && val != "0") {
        if (val == "_") {
          if (invert) {
            printf("expressions `~_', `-_' not supported\n");
            return false;
          }
          mode |= CURRENT_MODE;
          continue;
        }
        if (in) {
          CM(ENABLE_PROCESSED_INPUT)
          CM(ENABLE_LINE_INPUT)
          CM(ENABLE_ECHO_INPUT)
          CM(ENABLE_WINDOW_INPUT)
          CM(ENABLE_MOUSE_INPUT)
          CM(ENABLE_INSERT_MODE)
          CM(ENABLE_QUICK_EDIT_MODE)
          CM(ENABLE_EXTENDED_FLAGS)
          CM(ENABLE_VIRTUAL_TERMINAL_INPUT)
          CM(ENABLE_AUTO_POSITION)
        } else {
          CM(ENABLE_PROCESSED_OUTPUT)
          CM(ENABLE_WRAP_AT_EOL_OUTPUT)
          CM(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
          CM(DISABLE_NEWLINE_AUTO_RETURN)
          CM(ENABLE_LVB_GRID_WORLDWIDE)
        }
        printf("unknown option %s\n", val.c_str());
        return false;
      } else if (add) {
        mode |= invert ? ~hex : hex;
      } else {
        mask &= invert ? ~hex : hex;
      }
    }
  }

  return true;
}

bool set_in(const char *str, DWORD &mode, DWORD &mask) {
  bool ok = set_mode<true>(str, mode, mask);
  if ((mode | ~mask) & (ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE)) {
    mode |= ENABLE_EXTENDED_FLAGS;
    mask |= ENABLE_EXTENDED_FLAGS;
  }
  return ok;
}

bool set_out(const char *str, DWORD &mode, DWORD &mask) {
  return set_mode<false>(str, mode, mask);
}

#undef CM

int main(int argc, char *argv[]) {
  DWORD setin = 0;
  DWORD setout = 0;
  DWORD inmask;
  DWORD outmask;
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
             "           = = set a custom mode in hex or by name(s)\n"
             "Options:   -p pid = attach to another process\n"
             "           -l     = list valid values\n"
             "Custom modes:\n"
             " * Modes may be specified either using hex values (ex. 0x10) or by name.\n"
             " * To see a list of valid names, use `isvt -l'.\n"
             " * Modes can be combined using `+' and `-', or using `|', `&', and `~'.\n"
             "   The `+' operator is equivalent to `|'; `-' is equivalent to `&~'.\n"
             " * The current mode can be embedded in an expression; it is represented\n"
             "   by the `_' placeholder value.\n");
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

    if (arg[0] == '-' && arg[1] == 'l') {
      printf("Valid input modes:\n"
             "  ENABLE_PROCESSED_INPUT        (0x1)\n"
             "  ENABLE_LINE_INPUT             (0x2)\n"
             "  ENABLE_ECHO_INPUT             (0x4)\n"
             "  ENABLE_WINDOW_INPUT           (0x8)\n"
             "  ENABLE_MOUSE_INPUT            (0x10)\n"
             "  ENABLE_INSERT_MODE            (0x20)\n"
             "  ENABLE_QUICK_EDIT_MODE        (0x40)\n"
             "  ENABLE_EXTENDED_FLAGS        *(0x80)\n"
             "  ENABLE_AUTO_POSITION         *(0x100)\n"
             "  ENABLE_VIRTUAL_TERMINAL_INPUT (0x200)\n"
             "\n"
             "Valid output modes:\n"
             "  ENABLE_PROCESSED_OUTPUT            (0x1)\n"
             "  ENABLE_WRAP_AT_EOL_OUTPUT          (0x2)\n"
             "  ENABLE_VIRTUAL_TERMINAL_PROCESSING (0x4)\n"
             "  DISABLE_NEWLINE_AUTO_RETURN        (0x8)\n"
             "  ENABLE_LVB_GRID_WORLDWIDE          (0x10)\n"
             "\n"
             "Current mode placeholder:\n"
             "  _ (replaced with current input/output mode in an expression)\n"
             "\n"
             "* These flags are added automatically or not usually necessary.\n");
      return 0;
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
      DWORD mode;

      if (arg[0] == 'i') {
        if (!set_in(arg + 2, mode, inmask)) {
          printf("invalid input mode %s\n", arg);
          return 1;
        }
        setin = mode | CUSTOM_MODE;
      } else if (arg[0] == 'o') {
        if (!set_out(arg + 2, mode, outmask)) {
          printf("invalid output mode %s\n", arg);
          return 1;
        }
        setout = mode | CUSTOM_MODE;
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
  if (setin == 1) {
    SetConsoleMode(in, inmode | ENABLE_VIRTUAL_TERMINAL_INPUT);
  } else if (setin == -1) {
    SetConsoleMode(in, inmode & ~ENABLE_VIRTUAL_TERMINAL_INPUT);
  } else if (setin & CUSTOM_MODE) {
    if (setin & CURRENT_MODE) {
      setin |= inmode;
    }
    setin &= inmask & ~(CURRENT_MODE | CUSTOM_MODE);
    SetConsoleMode(in, setin);
    setin |= CUSTOM_MODE;
  }
  if (setout == 1) {
    SetConsoleMode(out, outmode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  } else if (setout == -1) {
    SetConsoleMode(out, outmode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  } else if (setout & CUSTOM_MODE) {
    if (setout & CURRENT_MODE) {
      setout |= outmode;
    }
    setout &= outmask & ~(CURRENT_MODE | CUSTOM_MODE);
    SetConsoleMode(out, setout & ~CUSTOM_MODE);
    setout |= CUSTOM_MODE;
  }

  FreeConsole();
  AttachConsole(ATTACH_PARENT_PROCESS);

  if (pid) {
    printf("Attached to process %u.\n", pid);
  }
  printf("cur[in]  = 0x%X\n", inmode);
  printf("         = ");
  print_in(inmode);
  printf("cur[out] = 0x%X\n", outmode);
  printf("         = ");
  print_out(outmode);

  if (setin | setout) {
    static const char *things[] = { "off", "", "on" };
    printf("----------\n");
    if (setin == -1 || setin == 1) {
      printf("vt[in]   = %s\n", things[setin+1]);
    } else if (setin & CUSTOM_MODE) {
      setin &= ~CUSTOM_MODE;
      printf("new[in]  = 0x%X\n", setin);
      printf("         = ");
      print_in(setin);
    }
    if (setout == -1 || setout == 1) {
      printf("vt[out]  = %s\n", things[setout+1]);
    } else if (setout & CUSTOM_MODE) {
      setout &= ~CUSTOM_MODE;
      printf("new[out] = 0x%X\n", setout);
      printf("         = ");
      print_out(setout);
    }
  }
  return 0;
}
