#ifndef UTIL_H
#define UTIL_H

#define print_to_debug(x) print_to(debugConsole, (x))
#define print_to_main(x) print_to(mainConsole, (x))
#define printf_to_debug(...) printf_to(debugConsole, __VA_ARGS__)
#define printf_to_main(...) printf_to(mainConsole, __VA_ARGS__)
#define print_xy_to_debug(...) print_xy_to(debugConsole, __VA_ARGS__)
#define print_xy(...) print_xy_to(mainConsole, __VA_ARGS__)
#define printf_xy_to_debug(...) printf_xy_to(debugConsole, __VA_ARGS__)
#define printf_xy(...) printf_xy_to(mainConsole, __VA_ARGS__)

extern PrintConsole *debugConsole, *mainConsole;

void clear_main();
void init_consoles(void);
void print_to(PrintConsole *c, char *str);
void printf_to(PrintConsole *c, char *format, ...);
void print_xy_to(PrintConsole *c, int x, int y, char *str);
void printf_xy_to(PrintConsole *c, int x, int y, char *format, ...);
void abort_msg(char *msg);

#endif
