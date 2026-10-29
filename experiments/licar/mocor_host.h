#ifndef PEAQ_LICAR_MOCOR_HOST_H
#define PEAQ_LICAR_MOCOR_HOST_H
#include "io_bridge.h"
#include "../runtime/mocor_files.h"
#include "../runtime/resident_tx_join.h"

#define PEAQ_LICAR_MOCOR_HOST_BYTES 1024u
typedef union { uint32_t alignment;uint8_t bytes[PEAQ_LICAR_MOCOR_HOST_BYTES]; } PeaqLicarMocorHost;
typedef struct {
 PeaqTxCreate create;
 PeaqTxThreadOp resume,terminate;
 PeaqTxInfo info;
 PeaqTxThreadOp delete_thread;
 uint32_t (*sleep)(uint32_t ticks);
 void (*sync_code)(void *,uint32_t);
} PeaqLicarMocorApi;
/* Original T45-V1.0 addresses, used only when a caller explicitly prepares a
 * resident host. No initializer here starts a thread, opens SD, or installs a
 * launcher. Sleep is in native ticks, NOT a physically calibrated millisecond.
 */
extern const PeaqLicarMocorApi peaq_pmp122_licar_native;
enum { PLMH_IDLE,PLMH_NO_THREAD,PLMH_RUNNING,PLMH_ROLLBACK,PLMH_QUARANTINED,PLMH_JOINED };
enum { PLMH_OK,PLMH_ARGUMENT,PLMH_CREATE,PLMH_RESUME,PLMH_IDENTITY,PLMH_ROLLBACK_ERROR,
       PLMH_PARK,PLMH_OPEN,PLMH_SIZE,PLMH_READ,PLMH_SEEK,PLMH_CLOSE,PLMH_REQUEST };
enum { PLMH_NO_SAVE=1,PLMH_NO_AUDIO=2 };
typedef struct {
 uint32_t phase,error,native_status,entered,unsupported,open_handles,close_uncertain;
 uint32_t read_calls,read_bytes,park_calls;
} PeaqLicarMocorStatus;

/* UI only; all arguments and code remain pinned until io_pump()==1. Storage
 * starts zeroed and is not reinitialized. Uses fixed files beneath D/E:\Games\
 * Racer: engine.pge, data, music.u8. D/E is the phone volume, never PC G/H.
 * All open calls are read-existing (0x31); this host NEVER writes any file.
 * Native save appends are explicitly unsupported; audio returns immediate
 * backpressure. These are remaining integration blockers, not fake success.
 * The bridge and runtime must be prepared before starting the returned host.
 */
int peaq_licar_mocor_prepare(PeaqLicarMocorHost *,const PeaqLicarMocorApi *,
 const PeaqFileApi *,PeaqLicarIoBridge *,const PeaqLicarIoMemory *,char drive);
const PeaqLicarIoHost *peaq_licar_mocor_services(PeaqLicarMocorHost *);
void peaq_licar_mocor_status(const PeaqLicarMocorHost *,PeaqLicarMocorStatus *);
/* True only when no native TCB or uncertain file handle is still owned. This
 * is diagnostic, NOT a replacement for the bridge/owner's full release gate.
 * Failed close poisons a handle and retains it; never retry an ambiguous close
 * against a potentially recycled native handle. A reboot may be necessary.
 */
int peaq_licar_mocor_quiescent(const PeaqLicarMocorHost *);
/* Own implementation of the already-used launcher cache sequence; privileged
 * ARM execution and physical cache coherence still need device qualification.
 */
void peaq_licar_mocor_sync_code(void *,uint32_t);
#endif
