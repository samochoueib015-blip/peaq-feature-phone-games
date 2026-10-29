#ifndef PEAQ_COLD_RESET_H
#define PEAQ_COLD_RESET_H
/* Bounded SC2720 watchdog arm. Does not return on successful reset. A failed
 * ADI transaction returns0 with memory still owned; callers must not jump back
 * into an OS image overwritten by the game arena. */
int peaq_cold_reset(void);
#endif
