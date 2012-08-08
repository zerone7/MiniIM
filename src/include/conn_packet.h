#ifndef _CONN_PACKET_H_
#define _CONN_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "conn_list.h"

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

/* packet list */
struct conn_packet_list {
	struct list_head list;
	struct conn_packet packet;
};

/* init the packet */
static inline void packet_init(struct conn_packet_list *packet)
{
	assert(packet);
	memset(packet, 0, sizeof(struct conn_packet_list) + MAX_PACKET_SIZE);
	INIT_LIST_HEAD(&packet->list);
}

/* convert header from network byte order to host byte order */
static inline void convert_header(struct conn_packet_list *packet)
{
	assert(packet);
	struct conn_packet *p = &packet->packet;
	p->length = ntohs(p->length);
	p->version = ntohs(p->version);
	p->command = ntohs(p->command);
	p->uin = ntohl(p->uin);
}

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
