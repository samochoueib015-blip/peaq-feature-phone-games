#ifndef PEAQ_BOOT_WATCHDOG_H
#define PEAQ_BOOT_WATCHDOG_H
#include <stdint.h>
typedef struct {uint32_t pmic,ap,changed;} PeaqBootWatchdog;
int peaq_boot_watchdog_suspend(PeaqBootWatchdog *);
int peaq_boot_watchdog_restore(PeaqBootWatchdog *);
#endif
