// The same file in both projects; see bitlink.h.

#include "bitlink.h"

#include <string.h>

#define MAGIC0 'B'
#define MAGIC1 '7'

static void put32(uint8_t *p, int32_t v) {
    const uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)u;
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
    p[3] = (uint8_t)(u >> 24);
}

static int32_t get32(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

size_t bl_encode(const bl_msg_t *m, uint8_t out[BL_FRAME_BYTES]) {
    out[0] = MAGIC0;
    out[1] = MAGIC1;
    out[2] = BL_VERSION;
    out[3] = m->type;
    out[4] = m->seq;
    out[5] = m->role;
    put32(out + 6, m->a);
    put32(out + 10, m->b);
    put32(out + 14, m->c);
    out[18] = m->d;
    out[19] = m->e;
    memcpy(out + 20, m->name, 8);
    return BL_FRAME_BYTES;
}

bool bl_decode(const uint8_t *data, size_t len, bl_msg_t *m) {
    if (data == NULL || len != BL_FRAME_BYTES) return false;
    if (data[0] != MAGIC0 || data[1] != MAGIC1 || data[2] != BL_VERSION) return false;
    if (data[3] == 0 || data[3] >= BL_TYPE_COUNT) return false;
    m->type = data[3];
    m->seq = data[4];
    m->role = data[5];
    m->a = get32(data + 6);
    m->b = get32(data + 10);
    m->c = get32(data + 14);
    m->d = data[18];
    m->e = data[19];
    memcpy(m->name, data + 20, 8);
    return true;
}

// --- Resending ----------------------------------------------------------------

static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

void bl_tx_init(bl_tx_t *tx, uint32_t seed) {
    memset(tx, 0, sizeof(*tx));
    tx->seq = (uint8_t)seed;
}

bool bl_tx_push(bl_tx_t *tx, const bl_msg_t *m, uint32_t give_up_ms, uint32_t now_ms) {
    if (tx->count >= BL_TX_SLOTS) return false;
    const uint8_t i = (uint8_t)((tx->head + tx->count) % BL_TX_SLOTS);
    tx->q[i] = *m;
    if (++tx->seq == 0) tx->seq = 1;
    tx->q[i].seq = tx->seq;
    tx->until[i] = now_ms + give_up_ms;
    // A frame joining an empty queue goes now; behind others it waits its turn.
    if (tx->count == 0 && !tx->in_flight) tx->next_try = now_ms;
    tx->count++;
    return true;
}

static void pop(bl_tx_t *tx, bl_msg_t *done) {
    if (done) *done = tx->q[tx->head];
    tx->head = (uint8_t)((tx->head + 1) % BL_TX_SLOTS);
    tx->count--;
    tx->in_flight = false;
}

bool bl_tx_due(bl_tx_t *tx, uint32_t now_ms, bl_msg_t *out) {
    if (tx->count == 0) return false;
    // The verdict never came: the send counts as a miss and goes again.
    if (tx->in_flight) {
        if (!due(now_ms, tx->sent_at + BL_RESULT_MS)) return false;
        tx->in_flight = false;
        tx->next_try = now_ms;
    }
    if (due(now_ms, tx->until[tx->head])) return false;  // bl_tx_expired's
    if (!due(now_ms, tx->next_try)) return false;
    *out = tx->q[tx->head];
    tx->in_flight = true;
    tx->sent_at = now_ms;
    return true;
}

bl_tx_outcome_t bl_tx_result(bl_tx_t *tx, bool acked, uint32_t now_ms, bl_msg_t *done) {
    if (!tx->in_flight || tx->count == 0) return BL_TX_PENDING;
    if (acked) {
        pop(tx, done);
        tx->next_try = now_ms;  // the next one may go at once
        return BL_TX_DELIVERED;
    }
    if (due(now_ms + BL_RETRY_MS, tx->until[tx->head])) {
        pop(tx, done);
        tx->next_try = now_ms;
        return BL_TX_FAILED;
    }
    tx->in_flight = false;
    tx->next_try = now_ms + BL_RETRY_MS;
    return BL_TX_PENDING;
}

bool bl_tx_expired(bl_tx_t *tx, uint32_t now_ms, bl_msg_t *done) {
    if (tx->count == 0 || tx->in_flight) return false;
    if (!due(now_ms, tx->until[tx->head])) return false;
    pop(tx, done);
    tx->next_try = now_ms;
    return true;
}

// --- Duplicates -----------------------------------------------------------------

void bl_rx_init(bl_rx_t *rx) { memset(rx, 0, sizeof(*rx)); }

bool bl_rx_fresh(bl_rx_t *rx, uint8_t seq) {
    if (seq == 0) return true;
    for (int i = 0; i < BL_RX_SEEN; i++)
        if (rx->seen[i] == seq) return false;
    rx->seen[rx->next] = seq;
    rx->next = (uint8_t)((rx->next + 1) % BL_RX_SEEN);
    return true;
}

// --- Near or not ------------------------------------------------------------------

void bl_presence_init(bl_presence_t *p) { memset(p, 0, sizeof(*p)); }

bool bl_presence_note(bl_presence_t *p, uint32_t now_ms) {
    // Signed throughout: a time stamped by another task can be a moment ahead.
    const bool arrival = !p->heard || (int32_t)(now_ms - p->heard_at) >= (int32_t)BL_ARRIVAL_GAP_MS;
    p->heard = true;
    p->heard_at = now_ms;
    return arrival;
}

bool bl_presence_near(const bl_presence_t *p, uint32_t now_ms) {
    return p->heard && (int32_t)(now_ms - p->heard_at) < (int32_t)BL_NEAR_MS;
}
