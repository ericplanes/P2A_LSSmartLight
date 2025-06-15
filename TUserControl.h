#ifndef TUSERCONTROL_H
#define TUSERCONTROL_H

#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5          // UID size in bytes
#define USER_NOT_FOUND 0xFF // Return value when UID not found

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

BYTE USER_FindPositionByRFID(BYTE *rfid_uid);
// Pre: rfid_uid points to 5-byte UID array
// Post: If UID found, returns user position (0-N), else returns USER_NOT_FOUND

#endif
