#ifndef TUSERCONTROL_H
#define TUSERCONTROL_H

#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5          // UID size in bytes
#define USER_NOT_FOUND 0xFF // Return value when UID not found
#define NUM_USERS 4         // Number of registered users (exceeds minimum of 3)
#define NO_USER {0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// Hard-coded accepted UIDs (5 bytes each) - as per enunciat requirement
static const BYTE accepted_uids[NUM_USERS][UID_SIZE] = {
    {0xFF, 0x11, 0x11, 0x11, 0x11}, // User 0 - FAKE USER
    {0x33, 0xA1, 0x38, 0x14, 0xBE}, // User 1 - REAL USER
    {0xFF, 0x22, 0x22, 0x22, 0x22}, // User 2 - FAKE USER
    {0xE3, 0xA2, 0x0E, 0x2A, 0x65}  // User 3 - REAL USER
};

/* =======================================
 *         PUBLIC FUNCTION HEADERS
 * ======================================= */

BYTE USER_FindPositionByRFID(const BYTE *rfid_uid);
// Pre: rfid_uid points to 5-byte UID array
// Post: If UID found, returns user position (0-N), else returns USER_NOT_FOUND

const BYTE *USER_GetUserByPosition(BYTE position);
// Pre: position is a valid user position (0-N)
// Post: If position valid, returns pointer to UID array, else returns NULL

#endif
