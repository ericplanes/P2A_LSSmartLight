#include "TUserControl.h"

/* =======================================
 *         PRIVATE FUNCTION HEADERS
 * ======================================= */
static BOOL is_user_equals(const BYTE *uid1, const BYTE *uid2);

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

// Hard-coded accepted UIDs (5 bytes each) - as per enunciat requirement
static const BYTE accepted_uids[NUM_USERS][UID_SIZE] = {
    {0x11, 0x04, 0x19, 0x94, 0xE0}, // User 0 - Example from enunciat
    {0x22, 0x15, 0x2A, 0x85, 0xD1}, // User 1
    {0x33, 0x26, 0x3B, 0x76, 0xC2}, // User 2
    {0x44, 0x37, 0x4C, 0x67, 0xB3}  // User 3
};

/* =======================================
 *         PUBLIC FUNCTION BODIES
 * ======================================= */

BYTE USER_FindPositionByRFID(BYTE *rfid_uid)
{
    // Search through all registered users
    for (BYTE i = 0; i < NUM_USERS; i++)
    {
        if (is_user_equals(rfid_uid, accepted_uids[i]))
        {
            // UID found - set output parameters
            return i; // Position in arrays
        }
    }

    // UID not found in registered users
    return USER_NOT_FOUND;
}

BYTE USER_GetUserByPosition(BYTE position)
{
    if (position < NUM_USERS)
    {
        return accepted_uids[position];
    }
    return USER_NOT_FOUND;
}

/* =======================================
 *         PRIVATE FUNCTIONS
 * ======================================= */

// Pre: uid1 and uid2 point to valid UID_SIZE byte arrays
// Post: Returns TRUE if UIDs match, FALSE otherwise
static BOOL is_user_equals(const BYTE *uid1, const BYTE *uid2)
{
    BYTE xor_result = 0;

    for (BYTE i = 0; i < UID_SIZE; i++)
    {
        xor_result |= (uid1[i] ^ uid2[i]);
        if (xor_result != 0)
        {
            return FALSE; // Early exit - UIDs don't match
        }
    }
    return TRUE;
}
