#include <string.h>

#include "warpcore.h"
#include "icmp.h"
#include "ip.h"


// Send the modified ICMP packet in the current receive buffer.
static void icmp_tx(struct warpcore * w, char * const buf,
             const uint16_t off, const uint16_t len)
{
	struct icmp_hdr * const icmp = (struct icmp_hdr * const)(buf + off);
	log(3, "ICMP type %d, code %d", icmp->type, icmp->code);

	// calculate the new ICMP checksum
	icmp->cksum = 0;
	icmp->cksum = in_cksum(icmp, len);

	// do IP transmit preparation
	ip_tx_with_rx_buf(w, IP_P_ICMP, buf, len);
}


// Make an ICMP unreachable message with the given code out of the
// current received packet.
void icmp_tx_unreach(struct warpcore * w, const uint8_t code,
                     char * const buf, const uint16_t off)
{
	// copy IP hdr + 64 bytes of the original IP packet as the ICMP payload
	struct ip_hdr * const ip =
		(struct ip_hdr * const)(buf + sizeof(struct eth_hdr));
	const uint16_t len = ip->hl * 4 + 64;
	// use memmove (instead of memcpy), since the regions overlap
	memmove(buf + off + sizeof(struct icmp_hdr) + 4, ip, len);

	// insert an ICMP header and set the fields
	struct icmp_hdr * const icmp = (struct icmp_hdr * const)(buf + off);
	icmp->type = ICMP_TYPE_UNREACH;
	icmp->code = code;

	// TODO: implement RFC4884 instead of setting the padding to zero
	uint32_t * const p = (uint32_t *)(buf + off + sizeof(struct icmp_hdr));
	*p = 0;

	icmp_tx(w, buf, off, sizeof(struct icmp_hdr) + 4 + len); // does cksum
}


// Handle an incoming ICMP packet, and optionally respond to it.
void icmp_rx(struct warpcore * w, char * const buf,
             const uint16_t off, const uint16_t len)
{
	struct icmp_hdr * const icmp = (struct icmp_hdr * const)(buf + off);
	log(3, "ICMP type %d, code %d", icmp->type, icmp->code);

	// validate the ICMP checksum
	if (in_cksum(icmp, len) != 0) {
		log(1, "invalid ICMP checksum, received %x", icmp->cksum);
		return;
	}

	switch (icmp->type) {
	case ICMP_TYPE_ECHO:
		// transform the received echo into an echo reply and send it
		icmp->type = ICMP_TYPE_ECHOREPLY;
		icmp_tx(w, buf, off, len);
		break;
	default:
		die("unhandled ICMP type %d", icmp->type);
		break;
	}
}
