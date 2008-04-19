// Includes
#include <PA9.h>       // Include for PA_Lib

#define DEBUG 1

#define MAX_Y_TEXT 24	// Number of vertical tiles
#define MAX_X_TEXT 33	// Number of horiz tiles
// Current console text
char console[MAX_Y_TEXT][MAX_X_TEXT];
int console_last = 0; /* index to the last added entry */
int console_screen = 0, console_bg = 0;

void init_console(int screen, int bgnum)
{
	int i;
	
	console_screen = screen;
	console_bg = bgnum;
	PA_InitText(screen, bgnum);
	PA_SetTextCol(screen, 31, 31, 31);

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
		PA_OutputText(console_screen, console_bg, pos, console[i]);
		if (--i < 0) i = MAX_Y_TEXT-1;
	} while (pos);

	return 0;
}

void abort_msg(char *msg)
{
	print_to_console(msg);
	print_to_console("Looping...");
	while(1) PA_WaitForVBL();
}

struct AP_HT_Entry {
	struct AP_HT_Entry *next;
	Wifi_AccessPoint *ap;
};

struct AP_HT_Entry *ap_ht[256] = {NULL};

struct AP_HT_Entry *entry_from_ap(Wifi_AccessPoint *ap)
{
	struct AP_HT_Entry *res;

	res = (struct AP_HT_Entry *) malloc(sizeof(struct AP_HT_Entry));

	if (!res)
		abort_msg("Alloc failed !");

	res->ap = ap;
	res->next = NULL;

	return res;	
}

struct AP_HT_Entry *find_ap(u8 *macaddr)
{
	struct AP_HT_Entry *ht_entry;
	int key = macaddr[5];
	char notpresent;

	if (ap_ht[key]) {
		ht_entry = ap_ht[key];
		while ((notpresent = memcmp(macaddr, ht_entry->ap->macaddr, 6)) != 0 && ht_entry->next)
			ht_entry = ht_entry->next;
		if(!notpresent)
			return ht_entry;
	} 
	return NULL;
}

int insert_ap(Wifi_AccessPoint *ap)
{
	int key	= ap->macaddr[5];
	Wifi_AccessPoint *ap_copy;
	struct AP_HT_Entry *ht_entry;
	char notpresent;
#ifdef DEBUG
	static int numap = 0;
	static char num[MAX_X_TEXT];
#endif

	ap_copy = (Wifi_AccessPoint *) malloc(sizeof(Wifi_AccessPoint));

	if (!ap_copy)
		abort_msg("Alloc failed !");

	memcpy(ap_copy, ap, sizeof(Wifi_AccessPoint));

	// check if there's already an entry in the hash table
	if (ap_ht[key] == NULL) {
		ap_ht[key] = entry_from_ap(ap_copy);
	} else {
		ht_entry = ap_ht[key];
		// Check if the AP is already present, walking the linked list
		while ((notpresent = memcmp(ap->macaddr, ht_entry->ap->macaddr, 6)) != 0 && ht_entry->next)
			ht_entry = ht_entry->next;

		if (notpresent)
			ht_entry->next = entry_from_ap(ap_copy);
		else {
			free(ap_copy);
			return 1;
		}
	}
#ifdef DEBUG
	numap++;
	sprintf(num, "%d AP found !", numap);
	print_to_console(num);
#endif

	return 0;
}
int display_list(int index, int flags) {
	int i;
	int displayed, seen;
	struct AP_HT_Entry *cur;
	char info1[MAX_X_TEXT];
	char info2[MAX_X_TEXT];

	PA_InitText(0,0);


	displayed = 0; seen = 0;
	PA_OutputSimpleText(0,0,0,"--------------------------------");
	for(i=0; i<256; i++) {
		cur = ap_ht[i];
		while(cur) {
			if(seen++ >= index) {
				snprintf(info1, MAX_X_TEXT, "%s", cur->ap->ssid);
				snprintf(info2, MAX_X_TEXT, "M:%02X%02X%02X%02X%02X%02X %04s",
					cur->ap->macaddr[0], cur->ap->macaddr[1], cur->ap->macaddr[2],
					cur->ap->macaddr[3], cur->ap->macaddr[4], cur->ap->macaddr[5],
					"WPA2");
				PA_OutputSimpleText(0, 0, displayed*3+1, info1);
				PA_OutputSimpleText(0, 0, displayed*3+2, info2);
				displayed++;
			}
			cur = cur->next;
		}
	}
	PA_OutputSimpleText(0,0,MAX_Y_TEXT-1,"--------------------------------");
	return 0;
}

int wardriving_loop()
{
	int num_aps, i;
	Wifi_AccessPoint cur_ap;

	// Set scan mode
	print_to_console("Setting scan mode...");
	Wifi_ScanMode();

#ifdef DEBUG
	char mac1[6]={0,1,2,3,4,5};
	char mac2[6]={5,4,3,2,1,0};

	memset(&cur_ap, 0, sizeof(cur_ap));
	strcpy(cur_ap.ssid, "OzoneParis.net : Accès Libre");
	cur_ap.ssid_len=strlen(cur_ap.ssid);
	memcpy(cur_ap.bssid, mac1, 6);
	memcpy(cur_ap.macaddr, mac2, 6);

	insert_ap(&cur_ap);
#endif

	// Infinite loop to keep the program running
	while (1)
	{
		num_aps = Wifi_GetNumAP();
		for (i = 0; i < num_aps; i++) {
			if(Wifi_GetAPData(i, &cur_ap) != WIFI_RETURN_OK)
				continue;
			if(!insert_ap(&cur_ap))
				print_to_console(cur_ap.ssid);
		}
		if (Pad.Held.B)
			print_to_console("B");
		display_list(0, 0);
		PA_WaitForVBL();
	}
}


int main(int argc, char ** argv)
{
	PA_Init();    // Initializes PA_Lib
	PA_InitVBL(); // Initializes a standard VBL

	init_console(1,0);

	print_to_console("AirScan v0.1 by Raphael Rigo");
	print_to_console("inspired by wifi_lib_test v0.3a by Stephen Stair");
	print_to_console("Initializing Wifi...");
	PA_InitWifi();

	wardriving_loop();
	
	return 0;
} // End of main()
