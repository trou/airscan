//// Includes
#include <PA9.h>       // Include for PA_Lib

#define DEBUG

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
#ifdef DEBUG
char debug = 1;
#endif
int timeout = 0;
u32 curtick; // Current tick to handle timeout

// Setup an auto scrolling text screen
// @screen = screen id
// @bgnum = background number
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

// add a line to scrolling display wrapping lines if necessary
void print_to_console(char *str)
{
	int i, pos, len;

	len = strlen(str);
	while (len > 0) {	
		console_last = (console_last+1)%MAX_Y_TEXT;

		if (len >= MAX_X_TEXT-1) {
			memcpy(console[console_last], str, MAX_X_TEXT-1);
			str += MAX_X_TEXT-1;
		} else {
			// reset line to avoid garbage
			memset(console[console_last], ' ', MAX_X_TEXT-1);
			memcpy(console[console_last], str, len);
		}
		len -= 32;
	}

	i = console_last;
	pos = MAX_Y_TEXT;
	do {
		pos--;
		PA_OutputText(console_screen, console_bg, pos, console[i]);
		if (--i < 0) i = MAX_Y_TEXT-1;
	} while (pos);

	return;
}

void abort_msg(char *msg)
{
	print_to_console("Fatal error :");
	print_to_console(msg);
	while(1) PA_WaitForVBL();
}

struct AP_HT_Entry {
	struct AP_HT_Entry 	*next;
	u32			tick;
	Wifi_AccessPoint 	*ap;
};


struct AP_HT_Entry *ap_ht[256] = {NULL}; // hash table
unsigned int numap = 0;		  // number of APs

// Default allocation size for arrays
#define DEFAULT_ALLOC_SIZE 10
// Arrays of pointers for fast access
struct AP_HT_Entry **ap_opn, **ap_wep, **ap_wpa;
// Arrays size, to check if realloc is needed
int opn_size, wep_size, wpa_size;
// Number of entries in each array
int num_opn, num_wep, num_wpa;

// Copy data from internal wifi storage
// update tick
// insert ptr into opn/wep/wpa tables
struct AP_HT_Entry *entry_from_ap(Wifi_AccessPoint *ap)
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

	res->ap = ap_copy;
	res->tick = curtick;
	res->next = NULL;

	if (ap->flags&WFLAG_APDATA_WPA) {
		// realloc needed
		if (num_wpa >= wpa_size) {
			wpa_size *= 2;
			ap_wpa = (struct AP_HT_Entry **)realloc(ap_wpa, wpa_size);
			if (!ap_wpa) abort_msg("Alloc failed !");
#ifdef DEBUG
			if(debug) print_to_console("realloc'd wpa");
#endif
		}
		ap_wpa[num_wpa++] = res;
	} else {
		if (ap->flags&WFLAG_APDATA_WEP) {
			// realloc needed
			if (num_wep >= wep_size) {
				wep_size *= 2;
				ap_wep = (struct AP_HT_Entry **)realloc(ap_wep, wep_size);
				if (!ap_wep) abort_msg("Alloc failed !");
#ifdef DEBUG
			if(debug) print_to_console("realloc'd wep");
#endif
			}
			ap_wep[num_wep++] = res;
		} else {
			// realloc needed
			if (num_opn >= opn_size) {
				opn_size *= 2;
				ap_opn = (struct AP_HT_Entry **)realloc(ap_opn, opn_size);
				if (!ap_opn) abort_msg("Alloc failed !");
#ifdef DEBUG
			if(debug) print_to_console("realloc'd opn");
#endif
			}
			ap_opn[num_opn++] = res;
		}
	}

	return res;
}

bool inline macaddr_cmp(void *mac1, void *mac2)
{
	return (((u32 *)mac1)[0]==((u32 *)mac2)[0]) && (((u16 *)mac1)[2]==((u16 *)mac2)[2]);
}

int insert_ap(Wifi_AccessPoint *ap)
{
	int key	= ap->macaddr[5];
	struct AP_HT_Entry *ht_entry;
	int same;

	// check if there's already an entry in the hash table
	if (ap_ht[key] == NULL) {
		ap_ht[key] = entry_from_ap(ap);
	} else {
		ht_entry = ap_ht[key];
		// Check if the AP is already present, walking the linked list
		//same = macaddr_cmp(ap->macaddr, ht_entry->ap->macaddr);
		while ((same = macaddr_cmp(ap->macaddr, ht_entry->ap->macaddr)) == 0 && ht_entry->next)
			ht_entry = ht_entry->next;

		if (same == 0)
			ht_entry->next = entry_from_ap(ap);
		else {
			// AP is already there, just update data
			ht_entry->tick = curtick;
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

void display_entry(int line, struct AP_HT_Entry *entry, char *mode)
{
	char info[MAX_X_TEXT];

	snprintf(info, MAX_X_TEXT, "%s", entry->ap->ssid);
	PA_OutputSimpleText(0, 0, line*3, info);
	snprintf(info, MAX_X_TEXT, "%02X%02X%02X%02X%02X%02X %s c%02d %3d%% %lus",
		entry->ap->macaddr[0], entry->ap->macaddr[1], entry->ap->macaddr[2],
		entry->ap->macaddr[3], entry->ap->macaddr[4], entry->ap->macaddr[5],
		mode, entry->ap->channel, (entry->ap->rssi*100)/0xD0,
		(curtick-entry->tick)/1000);
	PA_OutputSimpleText(0, 0, line*3+1, info);
	PA_OutputSimpleText(0, 0, line*3+2, SCREEN_SEP);
}

int display_list(int index, int flags) {
	int i;
	int displayed, seen;
	char info[MAX_X_TEXT];
	char modes[12];
	int opn, wep, wpa;
#ifdef DEBUG
	char debugs[MAX_X_TEXT];
#endif

	PA_InitText(0,0);

	displayed = 1; seen = wpa = wep = opn = 0;

	modes[0]=0;
	if(flags&DISP_OPN) strcat(modes,"OPN+");
	if(flags&DISP_WEP) strcat(modes,"WEP+");
	if(flags&DISP_WPA) strcat(modes,"WPA+");
	modes[strlen(modes)-1]=0;

	snprintf(info, MAX_X_TEXT, "%d AP On:%s Tmot:%d", numap, modes, timeout);
	PA_OutputSimpleText(0,0,0, info);
	snprintf(info, MAX_X_TEXT, "OPN:%d WEP:%d WPA:%d index:%d", num_opn, num_wep, num_wpa, index);
	PA_OutputSimpleText(0,0,1, info);
	PA_OutputSimpleText(0,0,2, SCREEN_SEP);

	if (flags&DISP_OPN) {
#ifdef DEBUG
		snprintf(debugs, MAX_X_TEXT, "OPN : %d %d", index, displayed);
//		print_to_console(debugs);
#endif
		for (i=index; i < num_opn && displayed < 8; i++) {
			display_entry(displayed++, ap_opn[i], "OPN");
		}
		index -= num_opn;
		if (index < 0) index = 0;
	}
	if (flags&DISP_WEP) {
#ifdef DEBUG
		snprintf(debugs, MAX_X_TEXT, "WEP : %d %d", index, displayed);
//		print_to_console(debugs);
#endif
		for (i=index; i < num_wep && displayed < 8; i++) {
			display_entry(displayed++, ap_wep[i], "WEP");
		}
		index -= num_wep;
		if (index < 0) index = 0;
	}
	if (flags&DISP_WPA) {
#ifdef DEBUG
		snprintf(debugs, MAX_X_TEXT, "WPA : %d %d", index, displayed);
//		print_to_console(debugs);
#endif
		for (i=index; i < num_wpa && displayed < 8; i++) {
			display_entry(displayed++, ap_wpa[i], "WPA");
		}
	}
	return 0;
}

void clean_timeouts()
{
	struct AP_HT_Entry *cur, *prev;
	char msg[MAX_X_TEXT];
	int i;

	for(i=0; i<256; i++) {
		cur = ap_ht[i];
		prev = NULL;
		while(cur) {
			if (curtick-(cur->tick) > timeout) {
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
	u32 lasttick;

	// Set scan mode
	print_to_console("Setting scan mode...");
	Wifi_ScanMode();

	// Allocate arrays
	opn_size = wep_size = wpa_size = DEFAULT_ALLOC_SIZE;
	num_opn = num_wep = num_wpa = num_aps = 0;
	ap_opn = (struct AP_HT_Entry **) malloc(DEFAULT_ALLOC_SIZE*sizeof(struct AP_HT_Entry *));
	if (ap_opn == NULL) abort_msg("alloc failed (opn)");
	ap_wep = (struct AP_HT_Entry **) malloc(DEFAULT_ALLOC_SIZE*sizeof(struct AP_HT_Entry *));
	if (ap_wep == NULL) abort_msg("alloc failed (wep)");
	ap_wpa = (struct AP_HT_Entry **) malloc(DEFAULT_ALLOC_SIZE*sizeof(struct AP_HT_Entry *));
	if (ap_wpa == NULL) abort_msg("alloc failed (wpa)");

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
			insert_ap(&cur_ap);
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
#ifdef DEBUG
		if (Pad.Newpress.Y) {
			debug ^= 1;
			if (debug)
				print_to_console("Debug is now ON");
			else
				print_to_console("Debug is now OFF");
		}
#endif
		display_list(index, flags);
	}
	return 0;
}


int main(int argc, char ** argv)
{
	PA_Init();    // Initializes PA_Lib
	PA_InitVBL(); // Initializes a standard VBL

	// Setup logging console on top screen
	init_console(1,0);

	print_to_console("AirScan v0.1a by Raphael Rigo");
	print_to_console("inspired by wifi_lib_test v0.3a by Stephen Stair");
	print_to_console("");
	print_to_console("B: Toggle OPN");
	print_to_console("A: Toggle WEP");
	print_to_console("X: Toggle WPA");
#ifdef DEBUG
	print_to_console("Y: Toggle debug");
#endif
	print_to_console("Up/Down : scroll");
	print_to_console("Left/Right : Timeout -/+");
	print_to_console("");

	print_to_console("Initializing Wifi...");
	PA_InitWifi();

	wardriving_loop();
	
	return 0;
}
