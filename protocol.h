/**
 * protocol.h
 * Minimal ACK + exponential-backoff retry protocol.
 *
 * Packet format (fixed header):
 *   [0]     PACKET_TYPE   (DATA or ACK)
 *   [1]     SEQ_NUM       (0–255, wraps)
 *   [2]     PAYLOAD_LEN
 *   [3..N]  PAYLOAD
 *
 * Retry policy:
 *   - After TX, switch to RX and wait ACK_TIMEOUT_MS for an ACK.
 *   - If no ACK, retry up to MAX_RETRIES times with exponential backoff.
 *   - Backoff delay: BACKOFF_BASE_MS * 2^attempt (capped at BACKOFF_MAX_MS).
 *   - After MAX_RETRIES failures, log a FAILED_TX event and give up.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "sx1276.h"

#define PROTO_HEADER_LEN     3U
#define PROTO_MAX_PAYLOAD    (SX1276_MAX_PAYLOAD - PROTO_HEADER_LEN)

#define ACK_TIMEOUT_MS       2000U
#define MAX_RETRIES          4U
#define BACKOFF_BASE_MS      1000U
#define BACKOFF_MAX_MS       16000U

typedef enum {
    PKT_DATA = 0x01U,
    PKT_ACK  = 0x02U,
} PacketType;

typedef struct {
    PacketType type;
    uint8_t    seq;
    uint8_t    payload[PROTO_MAX_PAYLOAD];
    uint8_t    payload_len;
} Packet;

typedef struct {
    uint32_t sent;
    uint32_t acked;
    uint32_t failed;
    uint32_t retries_total;
} ProtoStats;

/**
 * proto_send_reliable()
 * Encode packet, transmit, wait for ACK, retry with backoff on failure.
 * Returns true if ACK received within MAX_RETRIES attempts.
 */
bool proto_send_reliable(SX1276 *radio, const uint8_t *payload,
                         uint8_t len, uint8_t seq, ProtoStats *stats);

/**
 * proto_receive()
 * Wait for an incoming DATA packet, decode it, auto-send ACK.
 * Returns true if a valid DATA packet was received.
 */
bool proto_receive(SX1276 *radio, Packet *out, uint32_t timeout_ms);

/**
 * proto_stats_print()
 */
void proto_stats_print(const ProtoStats *stats);

#endif /* PROTOCOL_H */
