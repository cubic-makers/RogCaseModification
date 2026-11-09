#include "motion_profile.h"

/* Fixed physical groups, starting at the DIN-connected LED; 25 groups / 118 LEDs. */
const uint8_t light_group_sizes[APP_LIGHT_GROUP_COUNT] = {
    5, 3, 4, 5, 4, 4, 3, 5, 4, 3, 4, 9, 4,
    7, 6, 8, 8, 3, 2, 2, 3, 3, 3, 8, 8
};

/* Row = physical group 1..25.
 * ACTION(closed brightness %, opened brightness %, start seconds, duration seconds).
 * Change start times to change ordering without changing physical groups.
 * Shutdown runs this same timeline backward from its CURRENT position. */
const motion_action_t light_group_actions[APP_LIGHT_GROUP_COUNT] = {
    ACTION(0, 100, 6, 1.2), // Group 01: LED 001-005 (1-based physical IDs)
    ACTION(0, 100, 7, 1.2), // Group 02: LED 006-008 (1-based physical IDs)
    ACTION(0, 100, 8, 1.2), // Group 03: LED 009-012 (1-based physical IDs)
    ACTION(0, 100, 8, 1.2), // Group 04: LED 013-017 (1-based physical IDs)
    ACTION(0, 100, 8, 1.2), // Group 05: LED 018-021 (1-based physical IDs)
    ACTION(0, 100, 9, 2), // Group 06: LED 022-025 (1-based physical IDs)
    ACTION(0, 100, 9, 2), // Group 07: LED 026-028 (1-based physical IDs)
    ACTION(0, 100, 9, 2), // Group 08: LED 029-033 (1-based physical IDs)
    ACTION(0, 100, 9, 2), // Group 09: LED 034-037 (1-based physical IDs)
    ACTION(0, 100, 7, 1.2), // Group 10: LED 038-040 (1-based physical IDs)
    ACTION(0, 100, 6, 1.2), // Group 11: LED 041-044 (1-based physical IDs)
    ACTION(0, 100, 5, 1.2), // Group 12: LED 045-053 (1-based physical IDs)
    ACTION(0, 100, 4, 1.2), // Group 13: LED 054-057 (1-based physical IDs)
    ACTION(0, 100, 5, 1.2), // Group 14: LED 058-064 (1-based physical IDs)
    ACTION(0, 100, 4, 1.2), // Group 15: LED 065-070 (1-based physical IDs)
    ACTION(0, 100, 3, 1.2), // Group 16: LED 071-078 (1-based physical IDs)
    ACTION(0, 100, 3, 1.2), // Group 17: LED 079-086 (1-based physical IDs)
    ACTION(0, 100, 2, 1.2), // Group 18: LED 087-089 (1-based physical IDs)
    ACTION(0, 100, 1, 1.3), // Group 19: LED 090-091 (1-based physical IDs)
    ACTION(0, 100, 1, 1.3), // Group 20: LED 092-093 (1-based physical IDs)
    ACTION(0, 100, 2, 1.2), // Group 21: LED 094-096 (1-based physical IDs)
    ACTION(0, 100, 2, 1.2), // Group 22: LED 097-099 (1-based physical IDs)
    ACTION(0, 100, 2, 1.2), // Group 23: LED 100-102 (1-based physical IDs)
    ACTION(0, 100, 3, 1.2), // Group 24: LED 103-110 (1-based physical IDs)
    ACTION(0, 100, 3, 1.2) // Group 25: LED 111-118 (1-based physical IDs)
};
