PrintConsole *debugConsole, *mainConsole;

void clear_main()
{
	consoleSelect(mainConsole);
	consoleClear();
}

void init_consoles(void)
{
	// debug console on top
	mainConsole = consoleDemoInit();
	debugConsole = (PrintConsole *) malloc(sizeof(PrintConsole));

	if (!debugConsole)
		exit(1);

	memcpy(debugConsole, consoleGetDefault(), sizeof(PrintConsole));
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG); 

	consoleInit(debugConsole, debugConsole->bgLayer, BgType_Text4bpp,
			BgSize_T_256x256, debugConsole->mapBase, 
			debugConsole->gfxBase, true, true);
#ifdef DEBUG
	printf("Test debug console\n");
#endif
	consoleSelect(mainConsole);
#ifdef DEBUG
	printf("Test main console\n");
#endif
	return;
}

void print_to(PrintConsole *c, char *str)
{
	consoleSelect(c);
	printf("%s\n", str);
}

void printf_to(PrintConsole *c, char *format, ...)
{
	va_list args;

	consoleSelect(c);
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void print_xy_to(PrintConsole *c, int x, int y, char *str)
{
	static char buffer[MAX_X_TEXT+9];

	consoleSelect(c);
	snprintf(buffer, MAX_X_TEXT+8, "\x1b[%d;%dH%s", y, x, str);
	iprintf(buffer);
}

void printf_xy_to(PrintConsole *c, int x, int y, char *format, ...)
{
	static char buffer[MAX_X_TEXT+1];
	static char buffer2[MAX_X_TEXT+9];
	va_list args;

	consoleSelect(c);
	va_start(args, format);

	vsnprintf(buffer, MAX_X_TEXT+1, format, args);
	snprintf(buffer2, MAX_X_TEXT+8, "\x1b[%d;%dH%s", y, x, buffer);
	iprintf(buffer2);
	va_end(args);
}

void abort_msg(char *msg)
{
	print_to_debug("Fatal error :");
	print_to_debug(msg);
	while(1) swiWaitForVBlank();
}
void print_to(PrintConsole *c, char *str)
{
	consoleSelect(c);
	printf("%s\n", str);
}

void printf_to(PrintConsole *c, char *format, ...)
{
	va_list args;

	consoleSelect(c);
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void print_xy_to(PrintConsole *c, int x, int y, char *str)
{
	static char buffer[MAX_X_TEXT+9];

	consoleSelect(c);
	snprintf(buffer, MAX_X_TEXT+8, "\x1b[%d;%dH%s", y, x, str);
	iprintf(buffer);
}

void printf_xy_to(PrintConsole *c, int x, int y, char *format, ...)
{
	static char buffer[MAX_X_TEXT+1];
	static char buffer2[MAX_X_TEXT+9];
	va_list args;

	consoleSelect(c);
	va_start(args, format);

	vsnprintf(buffer, MAX_X_TEXT+1, format, args);
	snprintf(buffer2, MAX_X_TEXT+8, "\x1b[%d;%dH%s", y, x, buffer);
	iprintf(buffer2);
	va_end(args);
}

void abort_msg(char *msg)
{
	print_to_debug("Fatal error :");
	print_to_debug(msg);
	while(1) swiWaitForVBlank();
}
