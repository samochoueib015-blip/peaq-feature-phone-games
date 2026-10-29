#ifndef PEAQ_EMERALD_BOOT_TICKET_H
#define PEAQ_EMERALD_BOOT_TICKET_H
#include <stdint.h>
#define PEAQ_BOOT_SECTOR 512u
#define PEAQ_BOOT_PAYLOAD 492u
#define PEAQ_BOOT_REQUEST 0x31514750u
typedef struct {
 void *cookie;
 int (*read)(void *,unsigned slot,uint8_t out[512]);
 int (*write)(void *,unsigned slot,const uint8_t data[512]);
 int (*sync)(void *);
} PeaqBootTicketIo;
/* A legacy PGA save payload: its existing two-slot store supplies the outer
 * PGS1 header. Both preallocated files must be exactly512 bytes and exclusively
 * audited before permitting cold writes. No FAT or directory modifications.
 */
void peaq_boot_request(uint8_t payload[492],uint32_t game_crc,uint32_t volume);
/* Read/validate both slots without writing. Same newest-slot and torn-record
 * rules as consume. An inactive newest record always suppresses older ones. */
int peaq_boot_peek(const PeaqBootTicketIo *,uint32_t *game_crc,
                  uint32_t *volume,uint8_t scratch[1536]);
/* 1=valid one-shot request consumed AND reread, 0=no request, -1=I/O/format.
 * The game may start only after1. Consumption updates the latest slot IN PLACE
 * to a valid inactive ticket. It never falls back to an older active ticket.
 * A torn consume returns an error and cannot launch until the OS creates a new
 * request. In particular a reset/error after consumption boots the normal OS.
 */
int peaq_boot_consume(const PeaqBootTicketIo *,uint32_t expected_crc,
                     uint32_t *volume,uint8_t scratch[1536]);
#endif
