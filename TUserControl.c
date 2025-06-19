#include "TUserControl.h"

/* =======================================
 *         PRIVATE FUNCTION HEADERS
 * ======================================= */
static BOOL is_user_equals(const BYTE *uid1, const BYTE *uid2);

/* =======================================
 *         PRIVATE VARIABLES
 * ======================================= */

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

const BYTE *USER_GetUserByPosition(BYTE position)
{
    if (position < NUM_USERS)
    {
        return accepted_uids[position];
    }
    return (const BYTE *)USER_NOT_FOUND;
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
