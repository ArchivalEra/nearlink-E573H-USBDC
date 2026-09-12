/*
 * hwsle_transport.h — SSAP transport adapter for the WS73 dongle.
 *
 * Maps SSAP PDUs onto /dev/hwsle as ACB data frames (datatype 0xA3,
 * tcid 0x0A = SLE_SMTC). Frame layout on the wire (from our hardware
 * verification + OHOS HCI analysis):
 *
 *   [0xA3] [tcid u16 LE] [len u16 LE] [payload...]
 *
 * SSAP_Recv is invoked for frames whose tcid == TCID_SLE_SMTC.
 */

#ifndef HWSLE_TRANSPORT_H
#define HWSLE_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

#define HWSLE_DEV "/dev/hwsle"

#define HCI_DATATYPE_CMD    0xA1
#define HCI_DATATYPE_EVENT  0xA2
#define HCI_DATATYPE_ACB    0xA3
#define HCI_DATATYPE_ICB    0xA4

#define TCID_SLE_CMTC 0x02   /* common management */
#define TCID_SLE_SMTC 0x0A   /* service management — SSAP rides here */
#define TCID_SLE_CUTC 0x1F   /* default unicast transport */

/* Callback for received SSAP data (tcid 0x0A). Return 0 on success. */
typedef int (*ssap_recv_fn)(const uint8_t *data, size_t len);

/* Open the transport (non-blocking); registers recv callback. Returns fd or -1. */
int hwsle_transport_open(ssap_recv_fn recv_cb);

/* Send an SSAP PDU as an ACB frame on tcid 0x0A. Returns bytes written or -1. */
int hwsle_transport_send_ssap(const uint8_t *pdu, size_t len);

/* Send a raw ACB data frame on the given tcid. */
int hwsle_transport_send_acb(uint16_t tcid, const uint8_t *payload, size_t len);

/* Send an HCI command (datatype 0xA1). Returns bytes written or -1. */
int hwsle_transport_send_hci_cmd(uint16_t opcode, const uint8_t *params, size_t plen);

/* Poll/read loop — call in a loop or thread; dispatches to the recv callback. */
void hwsle_transport_run(void);

/* Close the transport. */
void hwsle_transport_close(void);

#endif /* HWSLE_TRANSPORT_H */
