#include "st25tb_poller.h"
#include "st25tb_poller_i.h"

#include <nfc/protocols/nfc_poller_base.h>

#define TAG "ST25TBPoller"

typedef St25tbPollerState (*St25tbPollerStateHandler)(St25tbError* error, St25tbPoller* instance);

const St25tbData* st25tb_poller_get_data(St25tbPoller* instance) {
    furi_assert(instance);
    furi_assert(instance->data);

    return instance->data;
}

static St25tbPoller* st25tb_poller_alloc(Nfc* nfc) {
    furi_assert(nfc);

    St25tbPoller* instance = malloc(sizeof(St25tbPoller));
    instance->nfc = nfc;
    instance->state = St25tbPollerStateIdle;
    instance->tx_buffer = bit_buffer_alloc(ST25TB_POLLER_MAX_BUFFER_SIZE);
    instance->rx_buffer = bit_buffer_alloc(ST25TB_POLLER_MAX_BUFFER_SIZE);

    // RF configuration is the same as 14b
    nfc_config(instance->nfc, NfcModePoller, NfcTechIso14443b);
    nfc_set_guard_time_us(instance->nfc, ST25TB_GUARD_TIME_US);
    nfc_set_fdt_poll_fc(instance->nfc, ST25TB_FDT_FC);
    nfc_set_fdt_poll_poll_us(instance->nfc, ST25TB_POLL_POLL_MIN_US);
    instance->data = st25tb_alloc();

    instance->st25tb_event.data = &instance->st25tb_event_data;
    instance->general_event.protocol = NfcProtocolSt25tb;
    instance->general_event.event_data = &instance->st25tb_event;
    instance->general_event.instance = instance;

    return instance;
}

static void st25tb_poller_free(St25tbPoller* instance) {
    furi_assert(instance);

    furi_assert(instance->tx_buffer);
    furi_assert(instance->rx_buffer);
    furi_assert(instance->data);

    bit_buffer_free(instance->tx_buffer);
    bit_buffer_free(instance->rx_buffer);
    st25tb_free(instance->data);
    free(instance);
}

static void
    st25tb_poller_set_callback(St25tbPoller* instance, NfcGenericCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;
}

static St25tbPollerState
    st25tb_poller_return_to_idle_on_error(St25tbError* error, St25tbPollerState next_state) {
    if(*error == St25tbErrorNone) {
        return next_state;
    } else {
        return St25tbPollerStateIdle;
    }
}

static St25tbPollerState
    st25tb_poller_state_idle_handler(St25tbError* error, St25tbPoller* instance) {
    instance->st25tb_event.type = St25tbPollerEventTypeReady;

    *error = st25tb_poller_select(instance, NULL);

    return st25tb_poller_return_to_idle_on_error(error, St25tbPollerStateSelected);
}

static St25tbPollerState
    st25tb_poller_state_selected_handler(St25tbError* error, St25tbPoller* instance) {
    instance->st25tb_event.type = St25tbPollerEventTypeReadSuccessful;

    *error = st25tb_poller_read(instance, instance->data);

    return st25tb_poller_return_to_idle_on_error(error, St25tbPollerStateRead);
}

static St25tbPollerState
    st25tb_poller_state_read_handler(St25tbError* error, St25tbPoller* instance) {
    instance->st25tb_event.type = St25tbPollerEventTypeReadSuccessful;
    *error = St25tbErrorNone;
    return St25tbPollerStateRead;
}

static St25tbPollerStateHandler st25tb_poller_state_handlers[St25tbPollerStateNum] = {
    [St25tbPollerStateIdle] = st25tb_poller_state_idle_handler,
    [St25tbPollerStateSelected] = st25tb_poller_state_selected_handler,
    [St25tbPollerStateRead] = st25tb_poller_state_read_handler,
};

static NfcCommand st25tb_poller_run(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolInvalid);
    furi_assert(event.event_data);

    St25tbPoller* instance = context;
    NfcEvent* nfc_event = event.event_data;
    NfcCommand command = NfcCommandContinue;

    furi_assert(instance->state < St25tbPollerStateNum);

    if(nfc_event->type == NfcEventTypePollerReady) {
        St25tbError error;

        instance->state = st25tb_poller_state_handlers[instance->state](&error, instance);
        if(error == St25tbErrorNone) {
            instance->st25tb_event_data.error = error;
            command = instance->callback(instance->general_event, instance->context);
        } else {
            instance->st25tb_event.type = St25tbPollerEventTypeError;
            instance->st25tb_event_data.error = error;
            command = instance->callback(instance->general_event, instance->context);
            // Add delay to switch context
            furi_delay_ms(100);
        }
    }

    return command;
}

static bool st25tb_poller_detect(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);
    furi_assert(event.protocol == NfcProtocolInvalid);

    bool protocol_detected = false;
    St25tbPoller* instance = context;
    NfcEvent* nfc_event = event.event_data;
    furi_assert(instance->state == St25tbPollerStateIdle);

    if(nfc_event->type == NfcEventTypePollerReady) {
        St25tbError error = st25tb_poller_initiate(instance, NULL);
        protocol_detected = (error == St25tbErrorNone);
    }

    return protocol_detected;
}

const NfcPollerBase nfc_poller_st25tb = {
    .alloc = (NfcPollerAlloc)st25tb_poller_alloc,
    .free = (NfcPollerFree)st25tb_poller_free,
    .set_callback = (NfcPollerSetCallback)st25tb_poller_set_callback,
    .run = (NfcPollerRun)st25tb_poller_run,
    .detect = (NfcPollerDetect)st25tb_poller_detect,
    .get_data = (NfcPollerGetData)st25tb_poller_get_data,
};
