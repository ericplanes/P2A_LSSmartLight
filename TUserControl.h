#ifndef TUSERCONTROL_H
#define TUSERCONTROL_H

#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5          // UID size in bytes
#define USER_NOT_FOUND 0xFF // Return value when UID not found
#define NUM_USERS 4         // Number of registered users (exceeds minimum of 3)

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

BYTE USER_FindPositionByRFID(BYTE *rfid_uid);
// Pre: rfid_uid points to 5-byte UID array
// Post: If UID found, returns user position (0-N), else returns USER_NOT_FOUND

const BYTE *USER_GetUserByPosition(BYTE position);
// Pre: position is a valid user position (0-N)
// Post: If position valid, returns pointer to UID array, else returns NULL

#endif
