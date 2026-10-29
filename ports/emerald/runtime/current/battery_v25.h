#ifndef PEAQ_BATTERY_V25_H
#define PEAQ_BATTERY_V25_H
/* Passive SC2720 fuel-gauge voltage, specific to the captured PEAQ unit.
 * Call only at the outer uncached boundary with IRQ/FIQ masked. Returns
 * millivolts, or zero when unavailable. Never enables/reset/writes the PMIC,
 * reads efuses, changes charging, or calls an OS service. */
unsigned peaq_battery_millivolts(void);
#endif
