#ifndef DISPLAY_H
#define DISPLAY_H

#include "airscan.h"

#define SCREEN_SEP "--------------------------------"
#define MAX_Y_TEXT 24			/* Number of vertical tiles */
#define MAX_X_TEXT 33			/* Number of horiz tiles */

#define DISPLAY_LINES 8

/* Currently displayed APs */
extern struct AP_HT_Entry *cur_entries[DISPLAY_LINES];

void display_ap(Wifi_AccessPoint *ap);
void display_entry(int line, struct AP_HT_Entry *entry, char *mode);
void display_list(int index, int flags);

#endif
