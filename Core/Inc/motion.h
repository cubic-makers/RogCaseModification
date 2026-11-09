#ifndef MOTION_H
#define MOTION_H
#include "motion_profile.h"
int motion_action_valid(const motion_action_t *action);
int motion_profiles_valid(void);
/* Pure evaluator: full_scale <= 2000. Endpoints are percent of full_scale. */
uint16_t motion_action_value(const motion_action_t *action, uint32_t position_ms, uint16_t full_scale);
int motion_init(uint32_t now);
void motion_poll(uint32_t now);
int motion_set_open(int open, uint32_t now);
int motion_busy(void);
uint32_t motion_position_ms(void);
uint32_t motion_duration_ms(void);
#endif
