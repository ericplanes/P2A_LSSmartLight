#include "THora.h"

// Static variables for time keeping
static BYTE current_hour = 0;    // 0-99 hours
static BYTE current_minutes = 0; // 0-59 minutes

/* =======================================
 *          PUBLIC FUNCTION BODIES
 * ======================================= */

void HORA_Init(void)
{
    // Initialize timer
    TiResetTics(TI_HORA);

    // Initialize time to 00:00
    current_hour = 0;
    current_minutes = 0;
}

void HORA_Motor(void)
{
    // Check if one minute has passed
    if (TiGetTics(TI_HORA) >= ONE_MINUTE)
    {
        // Reset timer for next minute
        TiResetTics(TI_HORA);

        // Increment minutes
        current_minutes++;

        // Handle minute overflow
        if (current_minutes > 59)
        {
            current_hour++;
            current_minutes = 0;

            // Handle hour overflow (wrap to 0 after 99)
            if (current_hour > 99)
            {
                current_hour = 0;
            }
        }
    }
}

void HORA_GetTime(BYTE *hour, BYTE *minutes)
{
    *hour = current_hour;
    *minutes = current_minutes;
}

void HORA_SetTime(BYTE hour, BYTE minutes)
{
    // Validate and set hour (0-99)
    if (hour <= 99)
    {
        current_hour = hour;
    }

    // Validate and set minutes (0-59)
    if (minutes <= 59)
    {
        current_minutes = minutes;
    }

    // Reset timer when time is manually set
    TiResetTics(TI_HORA);
}
