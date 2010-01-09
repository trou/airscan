/**
 * AirScan - airscan.h
 *
 * Copyright 2008-2010 Raphaël Rigo
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
