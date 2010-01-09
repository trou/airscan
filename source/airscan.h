#ifndef AIRSCAN_H
#define AIRSCAN_H

#include <dswifi9.h>

#define DISP_OPN 1
#define DISP_WEP 2
#define DISP_WPA 4

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

struct AP_HT_Entry {
	struct AP_HT_Entry 	*next;
	u32			tick;
	Wifi_AccessPoint 	*ap;
	int			array_idx;
};

extern u32 curtick;
extern unsigned int numap;	/* total number of APs */
extern char modes[12];		/* display modes (OPN/WEP/WPA) */
extern int timeout ;		/* number of milliseconds for AP timeout */


extern int num_null[3];		/* Number of NULL entries in each array */
extern int first_null[3];	/* First NULL entry */
extern int num[3];
extern struct AP_HT_Entry **ap[3];
#endif
