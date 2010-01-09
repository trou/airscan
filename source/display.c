#include <netinet/in.h>
#include <nds.h>
#include <dswifi9.h>
#include "airscan.h"
#include "utils.h"
#include "display.h"

struct AP_HT_Entry *cur_entries[DISPLAY_LINES] = {NULL};

void display_ap(Wifi_AccessPoint *ap)
{
	static struct in_addr ip, gw, sn, dns1, dns2;
	int status;

	clear_main();

	status = Wifi_AssocStatus();

	print_xy(0, 0, "SSID :");
	print_xy(0, 1, ap->ssid);
	printf_xy(0, 2, "State : %s", ASSOCSTATUS_STRINGS[status]);
	if (status == ASSOCSTATUS_ASSOCIATED) {
		ip = Wifi_GetIPInfo(&gw, &sn, &dns1, &dns2);

		printf_xy(0, 3, "IP :     %s", inet_ntoa(ip));
		printf_xy(0, 4, "Subnet : %s", inet_ntoa(sn));
		printf_xy(0, 5, "GW :     %s", inet_ntoa(gw));
		printf_xy(0, 6, "DNS1 :   %s", inet_ntoa(dns1));
		printf_xy(0, 7, "DNS2 :   %s", inet_ntoa(dns2));
	}
}

/* Print "entry" on line "line" */
void display_entry(int line, struct AP_HT_Entry *entry, char *mode)
{

	if (line < DISPLAY_LINES)
		cur_entries[line] = entry;

	printf_xy(0, line*3, "%s", entry->ap->ssid);
	printf_xy(0, line*3+1, "%02X%02X%02X%02X%02X%02X %s c%02d %3dp %ds",
	  entry->ap->macaddr[0], entry->ap->macaddr[1], entry->ap->macaddr[2],
	  entry->ap->macaddr[3], entry->ap->macaddr[4], entry->ap->macaddr[5],
	  mode, entry->ap->channel, (entry->ap->rssi*100)/0xD0,
	  (curtick-entry->tick)/1000);
	print_xy(0, line*3+2, SCREEN_SEP);
}

void display_list(int index, int flags)
{
	int i;
	int displayed;		/* Number of items already displayed */

	/* header */
	displayed = 1;

	clear_main();

	printf_xy(0, 0, "%d AP On:%s Tmot:%03d", numap, modes, timeout/1000);
	printf_xy(0, 1, "OPN:%03d WEP:%03d WPA:%03d idx:%03d", num[OPN], 
			num[WEP], num[WPA], index);
	print_xy(0, 2, SCREEN_SEP);

	memset(cur_entries, 0, sizeof(cur_entries));

	if (flags&DISP_OPN) {
		i = (first_null[OPN] >= 0 && index > first_null[OPN] ?
			first_null[OPN] : index);
		for (;i<(num[OPN]+num_null[OPN]) && displayed < DISPLAY_LINES;
			i++) {
			if (ap[OPN][i])
				display_entry(displayed++, ap[OPN][i], "OPN");
		}
		index -= num[OPN];
		if (index < 0) index = 0;
	}
	if (flags&DISP_WEP) {
		i = (first_null[WEP] >= 0 && index > first_null[WEP] ?
			first_null[WEP] : index);
		for (; i<(num[WEP]+num_null[WEP]) && displayed < DISPLAY_LINES; 
			i++) {
			if (ap[WEP][i])
				display_entry(displayed++, ap[WEP][i], "WEP");
		}
		index -= num[WEP];
		if (index < 0) index = 0;
	}
	if (flags&DISP_WPA) {
		i = (first_null[WPA] >= 0 && index > first_null[WPA] ?
			first_null[WPA] : index);
		for (; i<(num[WPA]+num_null[WPA]) && displayed < DISPLAY_LINES; 
			i++) {
			if (ap[WPA][i])
				display_entry(displayed++, ap[WPA][i], "WPA");
		}
	}
	return;
}
