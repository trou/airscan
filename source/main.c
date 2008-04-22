// Includes
#include <PA9.h>       // Include for PA_Lib

//#define DEBUG 0

#define SCREEN_SEP "--------------------------------"

#define DISP_OPN 1
#define DISP_WEP 2
#define DISP_WPA 4

#define MAX_Y_TEXT 24	// Number of vertical tiles
#define MAX_X_TEXT 33	// Number of horiz tiles
// Current console text
char console[MAX_Y_TEXT][MAX_X_TEXT];
int console_last = 0; /* index to the last added entry */
int console_screen = 0, console_bg = 0;
char debug = 0;
int timeout = 10;

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
	struct AP_HT_Entry 	*next;
	u32			tick;
	Wifi_AccessPoint 	*ap;
};

struct AP_HT_Entry *ap_ht[256] = {NULL};
int numap = 0;

struct AP_HT_Entry *entry_from_ap(Wifi_AccessPoint *ap, u32 tick)
{
	struct AP_HT_Entry *res;
	Wifi_AccessPoint *ap_copy;

	ap_copy = (Wifi_AccessPoint *) malloc(sizeof(Wifi_AccessPoint));
	if (!ap_copy)
		abort_msg("Alloc failed !");

	memcpy(ap_copy, ap, sizeof(Wifi_AccessPoint));

	res = (struct AP_HT_Entry *) malloc(sizeof(struct AP_HT_Entry));
	if (!res)
		abort_msg("Alloc failed !");

	res->ap = ap;
	res->tick = tick;
	res->next = NULL;

	return res;	
}

/*struct AP_HT_Entry *find_ap(u8 *macaddr)
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
}*/

bool inline CmpMacAddr(void *mac1, void *mac2)
{
	return (((u32 *)mac1)[0]==((u32 *)mac2)[0]) && (((u16 *)mac1)[2]==((u16 *)mac2)[2]);
}

int insert_ap(Wifi_AccessPoint *ap, u32 tick)
{
	int key	= ap->macaddr[5];
	struct AP_HT_Entry *ht_entry;
	char notpresent;

	// check if there's already an entry in the hash table
	if (ap_ht[key] == NULL) {
		ap_ht[key] = entry_from_ap(ap, tick);
	} else {
		ht_entry = ap_ht[key];
		// Check if the AP is already present, walking the linked list
		while ((notpresent = CmpMacAddr(ap->macaddr, ht_entry->ap->macaddr))==false && ht_entry->next)
			ht_entry = ht_entry->next;

		if (notpresent)
			ht_entry->next = entry_from_ap(ap, tick);
		else {
			// AP is already there, just update data
			ht_entry->tick = tick;
			ht_entry->ap->channel = ap->channel;
			ht_entry->ap->rssi = ap->rssi;
			ht_entry->ap->flags = ap->flags;
			memcpy(ht_entry->ap->ssid, ap->ssid, ap->ssid_len > 32 ? 32 : ap->ssid_len);
			return 1;
		}
	}
	numap++;

	return 0;
}

int display_list(int index, int flags, int tick) {
	int i;
	int displayed, seen;
	struct AP_HT_Entry *cur;
	char info[MAX_X_TEXT];
	char mode[4];
	char modes[12];
	int opn, wep, wpa;

	PA_InitText(0,0);

	displayed = 1; seen = wpa = wep = opn = 0;

	modes[0]=0;
	if(flags&DISP_OPN) strcat(modes,"OPN+"); 
	if(flags&DISP_WEP) strcat(modes,"WEP+"); 
	if(flags&DISP_WPA) strcat(modes,"WPA+"); 
	modes[strlen(modes)-1]=0;

	snprintf(info, MAX_X_TEXT, "%d AP On:%s Tmot:%d", numap, modes, timeout);
	PA_OutputSimpleText(0,0,0, info);
	PA_OutputSimpleText(0,0,2, SCREEN_SEP);
	for(i=0; i<256; i++) {
		cur = ap_ht[i];
		while(cur) {
			if(seen++ >= index) {
				if (cur->ap->flags & WFLAG_APDATA_WPA) {
					wpa++;
					if (!(flags & DISP_WPA))
						goto next;
					strcpy(mode, "WPA");
				}
				else {
					if (cur->ap->flags & WFLAG_APDATA_WEP) {
						wep++;
						if (!(flags & DISP_WEP))
							goto next;
						strcpy(mode, "WEP");
					} else {
						opn++;
						if (!(flags & DISP_OPN))
							goto next;
						strcpy(mode, "OPN");
					}
				}
				snprintf(info, MAX_X_TEXT, "%s", cur->ap->ssid);
				PA_OutputSimpleText(0, 0, displayed*3, info);
				snprintf(info, MAX_X_TEXT, "%02X%02X%02X%02X%02X%02X %4s c%02d %3d%% %lus",
					cur->ap->macaddr[0], cur->ap->macaddr[1], cur->ap->macaddr[2],
					cur->ap->macaddr[3], cur->ap->macaddr[4], cur->ap->macaddr[5],
					mode, cur->ap->channel, (cur->ap->rssi*100)/0xD0, 
					(tick-cur->tick)/1000);
				PA_OutputSimpleText(0, 0, displayed*3+1, info);
				PA_OutputSimpleText(0, 0, displayed*3+2, SCREEN_SEP);
				displayed++;
			}
			next:
			cur = cur->next;
		}
	}
	snprintf(info, MAX_X_TEXT, "OPN:%d WEP:%d WPA:%d index:%d", opn, wep, wpa, index);
	PA_OutputSimpleText(0,0,1, info);
	return 0;
}

void clean_timeouts(u32 tick)
{
	struct AP_HT_Entry *cur, *prev;
	char msg[MAX_X_TEXT];
	int i;

	for(i=0; i<256; i++) {
		cur = ap_ht[i];
		prev = NULL;
		while(cur) {
			if (tick-(cur->tick) > timeout) {
				snprintf(msg, MAX_X_TEXT, "Timeout : %s", cur->ap->ssid);
				print_to_console(msg);
				if (prev)
					prev->next = cur->next;
				else
					ap_ht[i] = cur->next;
				free(cur->ap);
				free(cur);
				numap--;
			}
			prev = cur;
			cur = cur->next;
		}
	}
}

int wardriving_loop()
{
	int num_aps, i, index, flags;
	Wifi_AccessPoint cur_ap;
	char timerId;
	u32 lasttick, curtick;

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

	insert_ap(&cur_ap, 0);
#endif

	flags = DISP_WPA|DISP_OPN|DISP_WEP;
	index = 0;

	StartTime(true);
	timerId = NewTimer(true);
	lasttick = Tick(timerId);
	// Infinite loop to keep the program running
	while (1)
	{
		curtick = Tick(timerId);
		num_aps = Wifi_GetNumAP();
		for (i = 0; i < num_aps; i++) {
			if(Wifi_GetAPData(i, &cur_ap) != WIFI_RETURN_OK)
				continue;
			if(!insert_ap(&cur_ap, curtick) && debug)
				print_to_console(cur_ap.ssid);
		}
		// Check timeouts every second
		if (timeout && (curtick-lasttick > 1000)) {
			lasttick = Tick(timerId);
			clean_timeouts(lasttick);
		}

		// Wait for VBL just before key handling and redraw
		PA_WaitForVBL();
		if (Pad.Newpress.Right)
			timeout += 1000;
		if (Pad.Newpress.Left)
			if(timeout > 0) timeout -= 1000;
		
		if (Pad.Newpress.Down)
			index++;
		if (Pad.Newpress.Up)
			if(index>0) index--;

		if (Pad.Newpress.B)
			flags ^= DISP_OPN;
		if (Pad.Newpress.A)
			flags ^= DISP_WEP;
		if (Pad.Newpress.X)
			flags ^= DISP_WPA;

		if (Pad.Newpress.Y) {
			debug ^= 1;
			if (debug)
				print_to_console("Debug is now ON");
			else
				print_to_console("Debug is now OFF");
		}

		display_list(index, flags, curtick);
	}
	return 0;
}


int main(int argc, char ** argv)
{
	PA_Init();    // Initializes PA_Lib
	PA_InitVBL(); // Initializes a standard VBL

	init_console(1,0);

	print_to_console("AirScan v0.1 by Raphael Rigo");
	print_to_console("inspired by wifi_lib_test v0.3a by Stephen Stair");
	print_to_console("");
	print_to_console("B: Toggle OPN");
	print_to_console("A: Toggle WEP");
	print_to_console("X: Toggle WPA");
	print_to_console("Y: Toggle debug");
	print_to_console("Up/Down : scroll");
	print_to_console("Left/Right : Timeout -/+");
	print_to_console("");

	print_to_console("Initializing Wifi...");
	PA_InitWifi();

	wardriving_loop();
	
	return 0;
} // End of main()
