#pragma once

#include <lib/nfc/protocols/iso14443_3b/iso14443_3b_poller_i.h>

#include "iso14443_4b_poller.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISO14443_4B_POLLER_ATS_FWT_FC (12000)

typedef enum {
    Iso14443_4bPollerStateIdle,
    Iso14443_4bPollerStateReadAts,
    Iso14443_4bPollerStateError,
    Iso14443_4bPollerStateReady,

    Iso14443_4bPollerStateNum,
} Iso14443_4bPollerState;

typedef enum {
    Iso14443_4bPollerSessionStateIdle,
    Iso14443_4bPollerSessionStateActive,
    Iso14443_4bPollerSessionStateStopRequest,
} Iso14443_4bPollerSessionState;

typedef struct {
    uint32_t block_number;
} Iso14443_4bPollerProtocolState;

struct Iso14443_4bPoller {
    Iso14443_3bPoller* iso14443_3b_poller;
    Iso14443_4bPollerState poller_state;
    Iso14443_4bPollerSessionState session_state;
    Iso14443_4bPollerProtocolState protocol_state;
    Iso14443_4bError error;
    Iso14443_4bData* data;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    Iso14443_4bPollerEventData iso14443_4b_event_data;
    Iso14443_4bPollerEvent iso14443_4b_event;
    NfcGenericEvent general_event;
    NfcGenericCallback callback;
    void* context;
};

Iso14443_4bError iso14443_4b_process_error(Iso14443_3bError error);

const Iso14443_4bData* iso14443_4b_poller_get_data(Iso14443_4bPoller* instance);

Iso14443_4bError iso14443_4b_poller_halt(Iso14443_4bPoller* instance);

Iso14443_4bError iso14443_4b_poller_async_read_ats(Iso14443_4bPoller* instance, SimpleArray* data);

Iso14443_4bError iso14443_4b_poller_send_block(
    Iso14443_4bPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt);

#ifdef __cplusplus
}
#endif