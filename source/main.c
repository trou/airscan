/**
 * AirScan - main.c
 *
 * Copyright 2008 Raphaël Rigo
 *
 * For mails :
 * user : devel-nds
 * domain : syscall.eu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <nds.h>
#include <dswifi9.h>

#define DEBUG

#define SCREEN_SEP "--------------------------------"

#define DISP_OPN 1
#define DISP_WEP 2
#define DISP_WPA 4

#define MAX_Y_TEXT 24			/* Number of vertical tiles */
#define MAX_X_TEXT 33			/* Number of horiz tiles */

PrintConsole *debugConsole, *mainConsole;
int timeout = 0;
u32 curtick; 				/* Current tick to handle timeout */
char modes[12];				/* display modes (OPN/WEP/WPA) */
#ifdef DEBUG
	char debug = 1;
	char debug_str[255];
#endif

enum array_indexes {
	OPN = 0,
	WEP,
	WPA
};

enum states {
	STATE_SCANNING,
	STATE_AP_DISPLAY
};

enum display_states {
	STATE_PACKET,
	STATE_CONNECTING,
	STATE_CONNECTED,
	STATE_ERROR
};



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

	memcpy(debugConsole, consoleGetDefault(), sizeof(PrintConsole));
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG); 

	consoleInit(debugConsole, debugConsole->bgLayer, BgType_Text4bpp, BgSize_T_256x256, debugConsole->mapBase, debugConsole->gfxBase, true, true);
#ifdef DEBUG
	printf("Test debug console");
#endif
	consoleSelect(mainConsole);
#ifdef DEBUG
	printf("Test main console");
#endif
	return;
}

/* add a line to scrolling display wrapping lines if necessary */
void print_to_console(char *str)
{
	consoleSelect(debugConsole);
	printf("%s\n", str);
}

void print_xy(int x, int y, char *str)
{
	static char buffer[MAX_X_TEXT+9];

	consoleSelect(mainConsole);
	snprintf(buffer, MAX_X_TEXT+8, "\x1b[%d;%dH%s", y, x, str);
	iprintf(buffer);
}

void abort_msg(char *msg)
{
	print_to_console("Fatal error :");
	print_to_console(msg);
	while(1) swiWaitForVBlank();
}


u32 tick()
{
	return ((TIMER1_DATA*(1<<16))+TIMER0_DATA)/33;
}


struct AP_HT_Entry {
	struct AP_HT_Entry 	*next;
	u32			tick;
	Wifi_AccessPoint 	*ap;
	int			array_idx;
};


struct AP_HT_Entry *ap_ht[256] = {NULL};	/* hash table */
unsigned int numap = 0;				/* number of APs */

#define DISPLAY_LINES 8
/* Default allocation size for arrays */
#define DEFAULT_ALLOC_SIZE 100
/* Arrays of pointers for fast access */
struct AP_HT_Entry **ap[3];
/* Arrays size, to check if realloc is needed */
int sizes[3];
/* Number of entries in each array */
int num[3];
/* Number of NULL entries in each array */
int num_null[3];
/* First NULL entry */
int first_null[3];

/* Currently displayed APs */
struct AP_HT_Entry *cur_entries[DISPLAY_LINES] = {NULL};

void do_realloc(int type)
{
	/* realloc needed */
	if (num[type] >= sizes[type]) {
		sizes[type] += DEFAULT_ALLOC_SIZE;
		ap[type] = (struct AP_HT_Entry **)realloc(ap[type], sizes[type]);
		if (!ap[type]) abort_msg("Alloc failed !");
#ifdef DEBUG
		if(debug) print_to_console("realloc'd");
#endif
	}
}

/* Insert the new AP in the fast access list
   and update the NULL entries if needed */
void do_insert_fast(int type, struct AP_HT_Entry *new_ap)
{
	/* Any NULL entry (timeouts) ? */
	if (num_null[type] > 0) {
		num_null[type]--;
		new_ap->array_idx = first_null[type];
		ap[type][first_null[type]] = new_ap;
		if (num_null[type])
			while(ap[type][++first_null[type]]);
		else
			first_null[type] = -1;
	} else {
		new_ap->array_idx = num[type];
		ap[type][num[type]] = new_ap;
	}
	num[type]++;
}

/* Copy data from internal wifi storage
 * update tick
 * insert ptr into opn/wep/wpa tables
 */
struct AP_HT_Entry *entry_from_ap(Wifi_AccessPoint *ap)
{
	struct AP_HT_Entry *new_ht_ap;
	Wifi_AccessPoint *ap_copy;

	ap_copy = (Wifi_AccessPoint *) malloc(sizeof(Wifi_AccessPoint));
	if (!ap_copy)
		abort_msg("Alloc failed !");

	memcpy(ap_copy, ap, sizeof(Wifi_AccessPoint));

	new_ht_ap = (struct AP_HT_Entry *) malloc(sizeof(struct AP_HT_Entry));
	if (!new_ht_ap)
		abort_msg("Alloc failed !");

	new_ht_ap->ap = ap_copy;
	new_ht_ap->tick = curtick;
	new_ht_ap->next = NULL;

	if (ap_copy->flags&WFLAG_APDATA_WPA) {
		do_realloc(WPA);
		do_insert_fast(WPA, new_ht_ap);
	} else {
		if (ap_copy->flags&WFLAG_APDATA_WEP) {
			do_realloc(WEP);
			do_insert_fast(WEP, new_ht_ap);
		} else {
			do_realloc(OPN);
			do_insert_fast(OPN, new_ht_ap);
		}
	}

	return new_ht_ap;
}

bool inline macaddr_cmp(void *mac1, void *mac2)
{
	return (((u32 *)mac1)[0]==((u32 *)mac2)[0]) && (((u16 *)mac1)[2]==((u16 *)mac2)[2]);
}

/* Insert or update ap data in the hash table
 * returns 0 if the ap wasn't present
 * 1 otherwise
 */
char insert_ap(Wifi_AccessPoint *ap)
{
	int key	= ap->macaddr[5];
	struct AP_HT_Entry *ht_entry;
	char same;

	/* check if there's already an entry in the hash table */
	if (ap_ht[key] == NULL) {
		ap_ht[key] = entry_from_ap(ap);
	} else {
		ht_entry = ap_ht[key];
		/* Check if the AP is already present, walking the linked list */
		while (!(same = macaddr_cmp(ap->macaddr,ht_entry->ap->macaddr))
			&& ht_entry->next)
			ht_entry = ht_entry->next;

		if (same == 0)
			ht_entry->next = entry_from_ap(ap);
		else {
			/* AP is already there, just update data */
			ht_entry->tick = curtick;
			ht_entry->ap->channel = ap->channel;
			ht_entry->ap->rssi = ap->rssi;
			ht_entry->ap->flags = ap->flags;
			memcpy(ht_entry->ap->ssid, ap->ssid, 
				ap->ssid_len > 32 ? 32 : ap->ssid_len);
			return 1;
		}
	}
	numap++;

	return 0;
}

/* Print "entry" on line "line" */
void display_entry(int line, struct AP_HT_Entry *entry, char *mode)
{
	char info[MAX_X_TEXT];

	if (line < DISPLAY_LINES)
		cur_entries[line] = entry;

	snprintf(info, MAX_X_TEXT, "%s", entry->ap->ssid);
	print_xy(0, line*3, info);
	snprintf(info, MAX_X_TEXT, "%02X%02X%02X%02X%02X%02X %s c%02d %3d%% %ds",
		entry->ap->macaddr[0], entry->ap->macaddr[1], entry->ap->macaddr[2],
		entry->ap->macaddr[3], entry->ap->macaddr[4], entry->ap->macaddr[5],
		mode, entry->ap->channel, (entry->ap->rssi*100)/0xD0,
		(curtick-entry->tick)/1000);
	print_xy(0, line*3+1, info);
	print_xy(0, line*3+2, SCREEN_SEP);
}

void display_list(int index, int flags)
{
	int i;
	int displayed;		/* Number of items already displayed */
	char info[MAX_X_TEXT];

	/* header */
	displayed = 1;

	clear_main();

	snprintf(info, MAX_X_TEXT, "%d AP On:%s Tmot:%03d", numap, modes, timeout/1000);
	print_xy(0, 0, info);
	snprintf(info, MAX_X_TEXT, "OPN:%03d WEP:%03d WPA:%03d idx:%03d", num[OPN], num[WEP], num[WPA], index);
	print_xy(0, 1, info);
	print_xy(0, 2, SCREEN_SEP);

	memset(cur_entries, 0, sizeof(cur_entries));

	if (flags&DISP_OPN) {
		i = (first_null[OPN] >= 0 && index > first_null[OPN] ?
			first_null[OPN] : index);
		for (; i < (num[OPN]+num_null[OPN]) && displayed < DISPLAY_LINES; i++) {
			if (ap[OPN][i])
				display_entry(displayed++, ap[OPN][i], "OPN");
		}
		index -= num[OPN];
		if (index < 0) index = 0;
	}
	if (flags&DISP_WEP) {
		i = (first_null[WEP] >= 0 && index > first_null[WEP] ?
			first_null[WEP] : index);
		for (; i < (num[WEP]+num_null[WEP]) && displayed < DISPLAY_LINES; i++) {
			if (ap[WEP][i])
				display_entry(displayed++, ap[WEP][i], "WEP");
		}
		index -= num[WEP];
		if (index < 0) index = 0;
	}
	if (flags&DISP_WPA) {
		i = (first_null[WPA] >= 0 && index > first_null[WPA] ?
			first_null[WPA] : index);
		for (; i < (num[WPA]+num_null[WPA]) && displayed < DISPLAY_LINES; i++) {
			if (ap[WPA][i])
				display_entry(displayed++, ap[WPA][i], "WPA");
		}
	}
	return;
}

/* Delete APs which have timeouted
 * not working due to the way I changed the lists
 */
void clean_timeouts()
{
	struct AP_HT_Entry *cur, *prev;
	char msg[MAX_X_TEXT];
	int i, type, idx;


	/* walk the whole hash table */
	for(i = 0; i < 256; i++) {
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

				if (cur->ap->flags&WFLAG_APDATA_WPA) {
					type = WPA;
				} else {
					if (cur->ap->flags&WFLAG_APDATA_WEP)
						type = WEP;
					else 
						type = OPN;	
				}
				idx = cur->array_idx;

				ap[type][idx] = NULL;
				if (num_null[type] == 0 || idx < first_null[type])
					first_null[type] = idx;
				num_null[type]++;
				num[type]--;

				free(cur->ap);
				free(cur);
				numap--;
			}
			prev = cur;
			cur = cur->next;
		}
	}
}

void wardriving_loop()
{
	int num_aps, i, index, flags, pressed;
	touchPosition touchXY;
	Wifi_AccessPoint cur_ap;
	u32 lasttick;
	char state, display_state;
	/* Vars for AP_DISPLAY */
	int entry_n;
	struct AP_HT_Entry *entry = NULL;

	print_to_console("Setting scan mode...");
	Wifi_ScanMode();


	state = STATE_SCANNING;

	for (i = 0; i < 3; i++) {
		sizes[i] = DEFAULT_ALLOC_SIZE;
		num[i] = num_null[i] = 0;
		first_null[i] = -1;
		ap[i] = (struct AP_HT_Entry **) malloc(sizes[i]*sizeof(struct AP_HT_Entry *));
		if (ap[i] == NULL) abort_msg("alloc failed");
	}
	num_aps = 0;

	flags = DISP_WPA|DISP_OPN|DISP_WEP;
	strcpy(modes, "OPN+WEP+WPA");

	index = 0;

	
        TIMER0_CR = TIMER_ENABLE|TIMER_DIV_1024;
        TIMER1_CR = TIMER_ENABLE|TIMER_CASCADE;
	lasttick = tick();
	
	while (1)
	{
		switch (state) {
			case STATE_SCANNING:
		curtick = tick();
		scanKeys();
		pressed = keysDown();

		/* Handle stylus press to display more detailed infos 
 		 * handle this before AP insertion, to avoid race
		 * conditions */
		if (pressed & KEY_TOUCH) {
			touchRead(&touchXY);
			/* Entry number : 8 pixels for text, 3 lines */
			entry_n = touchXY.py/8/3;
			entry = cur_entries[entry_n];
#ifdef DEBUG
			sprintf(debug_str, "Entry : Y : %d", entry_n);
			print_to_console(debug_str);
#endif
			if (entry) {
				print_to_console(entry->ap->ssid);
				state = STATE_AP_DISPLAY;
			}
		}

		num_aps = Wifi_GetNumAP();
		for (i = 0; i < num_aps; i++) {
			if(Wifi_GetAPData(i, &cur_ap) != WIFI_RETURN_OK)
				continue;
			insert_ap(&cur_ap);
		}

		/* Check timeouts every second */
		if (timeout && (curtick-lasttick > 1000)) {
			lasttick = tick();
			clean_timeouts(lasttick);
		}

		/* Wait for VBL just before key handling and redraw */
		swiWaitForVBlank();
		if (pressed & KEY_RIGHT)
			timeout += 1000;
		if (pressed & KEY_LEFT && timeout > 0)
			timeout -= 1000;
		
		if (pressed & KEY_DOWN)
			index++;
		if (pressed & KEY_UP && index > 0)
			index--;
		if (pressed & KEY_R && index+(DISPLAY_LINES-1) <= num_aps)
			index += DISPLAY_LINES-1;
		if (pressed & KEY_L && index >= DISPLAY_LINES-1)
			index -= DISPLAY_LINES-1;

		if (pressed & KEY_B)
			flags ^= DISP_OPN;
		if (pressed & KEY_A)
			flags ^= DISP_WEP;
		if (pressed & KEY_X)
			flags ^= DISP_WPA;

		/* Update modes string */
		if (pressed & KEY_B || pressed & KEY_A || pressed & KEY_X) {
			modes[0] = 0;
			if(flags&DISP_OPN) strcat(modes,"OPN+");
			if(flags&DISP_WEP) strcat(modes,"WEP+");
			if(flags&DISP_WPA) strcat(modes,"WPA+");
			modes[strlen(modes)-1] = 0; /* remove the + */
		}

#ifdef DEBUG
		if (pressed & KEY_Y) {
			debug ^= 1;
			if (debug)
				print_to_console("Debug is now ON");
			else
				print_to_console("Debug is now OFF");
		}
#endif
		display_list(index, flags);
		break;

		case STATE_AP_DISPLAY:
			/* TODO:
			 * 1) default to packet display
			 * 2) try DHCP
			 * 3) try default IPs
			 * 4) handle WEP ?
			 */
			print_to_console("kikoo");
			/* Try to connect */
			if (!(entry->ap->flags&WFLAG_APDATA_WPA) &&
				!(entry->ap->flags&WFLAG_APDATA_WEP)
				&& display_state == STATE_PACKET) {
					int ret;
					Wifi_DisconnectAP();
					ret = Wifi_ConnectAP(entry->ap,
							WEPMODE_NONE, 0,
							NULL);
					if(ret)
						print_to_console("error connecting");
			}
			state = STATE_SCANNING;
			display_state = STATE_PACKET;
			break;
		}
	}
}


int main(int argc, char ** argv)
{

	irqInit();
	irqEnable(IRQ_VBLANK);

	/* Setup logging console on top screen */
	init_consoles();

	print_to_console("AirScan v0.2 by Raphael Rigo");
	print_to_console("inspired by wifi_lib_test v0.3a by Stephen Stair");
	print_to_console("");
	print_to_console("B: Toggle OPN");
	print_to_console("A: Toggle WEP");
	print_to_console("X: Toggle WPA");
#ifdef DEBUG
	print_to_console("Y: Toggle debug");
#endif
	print_to_console("Up/Down : scroll");
	print_to_console("Left/Right : Timeout -/+ (NOT WORKING)");
	print_to_console("");

	print_to_console("Initializing Wifi...");
	Wifi_InitDefault(INIT_ONLY);
	

	wardriving_loop();
	
	return 0;
}
