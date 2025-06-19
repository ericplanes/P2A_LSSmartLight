#ifndef TUSERCONTROL_H
#define TUSERCONTROL_H

#include "Utils.h"

/* =======================================
 *              CONSTANTS
 * ======================================= */

#define UID_SIZE 5          // UID size in bytes
#define USER_NOT_FOUND 0xFF // Return value when UID not found
#define NUM_USERS 4         // Number of registered users (exceeds minimum of 3)

// Hard-coded accepted UIDs (5 bytes each) - as per enunciat requirement
static const BYTE accepted_uids[NUM_USERS][UID_SIZE] = {
    {0x11, 0x04, 0x19, 0x94, 0xE0}, // User 0 - Example from enunciat
    {0x22, 0x15, 0x2A, 0x85, 0xD1}, // User 1
    {0x33, 0x26, 0x3B, 0x76, 0xC2}, // User 2
    {0x44, 0x37, 0x4C, 0x67, 0xB3}  // User 3
};

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
