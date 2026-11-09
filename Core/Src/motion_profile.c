#include "motion_profile.h"

/* Row = Servo1..6: (closed %, opened %, opening start seconds, duration seconds).
 * Retain the previous full-travel bench defaults until each mechanism is calibrated.
 * Example only: ACTION(20, 60, 5.6, 9.0) -> 36 to 108 degrees, 5.6..14.6 s. */
const motion_action_t servo_actions[APP_SERVO_ACTION_COUNT] = {
    ACTION(100, 80, 7, 1.2),
    ACTION(0, 30, 8, 0.8),
    ACTION(0, 20, 7, 1.2),
    ACTION(100, 30, 0.09, 1),
    ACTION(0, 100, 1, 1.2),
    ACTION(0, 70, 0, 1)
};
