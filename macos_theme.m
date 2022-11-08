#import <AppKit/AppKit.h>
#include <stdio.h>
#include <string.h>

static void usage() {
  puts("Usage: macos_theme [-n | --name]\n"
       "No args     - Prints current theme, 'dark' or 'light'.\n"
       "-n | --name - Prints current NSAppearanceName.");
}

int main(int argc, char **argv) {
  NSString *light = @"NSAppearanceNameAqua";
  NSString *dark = @"NSAppearanceNameDarkAqua";
  NSApplication *app = [NSApplication sharedApplication];
  NSAppearance *appearance = [app effectiveAppearance];

  if (argc == 1) {
    NSAppearanceName bestFit =
      [appearance bestMatchFromAppearancesWithNames:@[light, dark]];
    puts([bestFit characterAtIndex:16] == 'D' ? "dark" : "light");
  } else if (argc == 2) {
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
      usage();
    } else if (!strcmp(argv[1], "-n") || !strcmp(argv[1], "--name")) {
      NSAppearanceName name = [appearance name];
      puts(name.UTF8String);
    } else {
      usage();
      return 1;
    }
  } else {
    usage();
    return 1;
  }

  return 0;
}
