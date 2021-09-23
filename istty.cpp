#ifdef _WIN32
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <cstdio>

void usage() {
  printf("Usage: istty -[p][ioe]\n"
         "-[ioe] = select input, output, or error stream\n"
         "-p     = print result (y/n) instead of returning exit code\n");
}

int main(int argc, char *argv[]) {
  if (argc != 2 || argv[1][0] != '-') {
    usage();
    return 2;
  }

  bool print = false;
  char stream = argv[1][1];
  if (argv[1][1] == 'p') {
    print = true;
    stream = argv[1][2];
    if (!stream || argv[1][3]) {
      usage();
      return 2;
    }
  } else if (!stream || argv[1][2]) {
    usage();
    return 2;
  }

#ifdef _WIN32
  HANDLE handle;
  switch (stream) {
    case 'i':
      handle = GetStdHandle(STD_INPUT_HANDLE);
      break;
    case 'o':
      handle = GetStdHandle(STD_OUTPUT_HANDLE);
      break;
    case 'e':
      handle = GetStdHandle(STD_ERROR_HANDLE);
      break;
    default:
      usage();
      return 2;
  }
  bool result = GetFileType(handle) == FILE_TYPE_CHAR;
#else
  int fileno;
  switch (stream) {
    case 'i':
      fileno = STDIN_FILENO;
      break;
    case 'o':
      fileno = STDOUT_FILENO;
      break;
    case 'e':
      fileno = STDERR_FILENO;
      break;
    default:
      usage();
      return 2;
  }
  bool result = isatty(fileno);
#endif

  if (result) {
    if (print) {
      printf("y");
    }
    return 0;
  } else {
    if (print) {
      printf("n");
      return 0;
    }
    return 1;
  }
}