#ifndef TUSERCONTROL_H
#define TUSERCONTROL_H

#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5 // UID size in bytes

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

BOOL USER_FindByRFID(BYTE *rfid_uid, BYTE *user_id, BYTE *user_position);
// Pre: rfid_uid points to 5-byte UID array, user_id and user_position point to valid BYTE variables
// Post: If UID found, fills user_id and user_position and returns TRUE
//       If UID not found, returns FALSE

#endif
