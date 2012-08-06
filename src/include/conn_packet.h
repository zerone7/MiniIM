#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>

/*
 * The packet structure used by connection server
 */
struct conn_packet {
	uint16_t length;	/* length of this packet */
	uint16_t version;	/* version of our protocol */
	uint16_t command;	/* command which the client send */
	uint16_t pad;		/* pad 2 bytes, for alignment */
	uint32_t uin;		/* uin of the user */
	uint8_t parameters[0];	/* command parameters */
} __attribute__((__packed__));

/* get length of the packet */
static inline uint16_t get_length(struct conn_packet *packet)
{
	assert(packet);
	return packet->length;
}

/* get version of the protocol */
static inline uint16_t get_version(struct conn_packet *packet)
{
	assert(packet);
	return packet->version;
}

/* get command of the packet */
static inline uint16_t get_command(struct conn_packet *packet)
{
	assert(packet);
	return packet->command;
}

/* get uin of the packet */
static inline uint32_t get_uin(struct conn_packet *packet)
{
	assert(packet);
	return packet->uin;
}

/* get parameters of the command */
static inline uint8_t* get_parameters(struct conn_packet *packet)
{
	assert(packet);
	return packet->parameters;
}

#endif
