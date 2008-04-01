// Includes
#include <PA9.h>       // Include for PA_Lib

#define MAX_Y_TEXT 24
#define MAX_X_TEXT 33
char console[MAX_Y_TEXT][MAX_X_TEXT];
int console_last = 0; /* index to the last added entry */

void init_console()
{
	int i;

	PA_InitText(1, 0);
	for (i = 0; i < MAX_Y_TEXT; i++)
		console[i][MAX_X_TEXT-1] = 0;
		
}

// add a line to scrolling display on screen 1
// wrapping lines if necessary
int print_to_console(char *str)
{
	int i, pos, len;


	len = strlen(str);
	do {	
		console_last = (console_last+1)%MAX_Y_TEXT;

		if (len >= 32) {
			memcpy(console[console_last], str, 32);
			str += 32;
		} else {
			memset(console[console_last], ' ', MAX_X_TEXT-1);
			memcpy(console[console_last], str, len > 32 ? 32 : len);
		}
		len -= 32;
	} while (len > 0);

	i = console_last;
	pos = MAX_Y_TEXT;
	do {
		pos--;
		PA_OutputText(1, 0, pos, console[i]);
		if (--i < 0) i = MAX_Y_TEXT-1;
	} while (pos);

	return 0;
}


// Function: main()
int main(int argc, char ** argv)
{
	int i;
	char num[10];

	PA_Init();    // Initializes PA_Lib
	PA_InitVBL(); // Initializes a standard VBL
	init_console();

	print_to_console("AirScan v0.1 by Raphael Rigo");
//	print_to_console("wifi_lib_test v0.3a by Stephen Stair");
	print_to_console("Initializing Wifi...");
	PA_InitWifi();


	// Set scan mode
	print_to_console("Setting scan mode...");
	Wifi_ScanMode();
	
	// Infinite loop to keep the program running
	while (1)
	{
		i = Wifi_GetNumAP();
		if (i) {
			sprintf(num, "%d", i);
			print_to_console(num);
		}
		PA_WaitForVBL();
	}
	
	return 0;
} // End of main()
