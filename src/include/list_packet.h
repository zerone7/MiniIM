#ifndef _LIST_PACKET_H_
#define _LIST_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "list.h"

#define LIST_PACKET_SIZE	(sizeof(struct list_head) + MAX_PACKET_LEN)

/* packet list */
struct list_packet {
	struct list_head list;
	struct packet packet;
};

/* get length of the packet */
static inline uint16_t get_length_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.len;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.len);
}

/* get version of the protocol */
static inline uint16_t get_version_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.ver;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.ver);
}

/* get command of the packet */
static inline uint16_t get_command_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.cmd;
	/* TODO: need to change to network byte order */
	//return ntohs(packet->packet.cmd);
}

/* get uin of the packet */
static inline uint32_t get_uin_host(const struct list_packet *packet)
{
	assert(packet);
	return packet->packet.uin;
	/* TODO: need to change to network byte order */
	//return ntohl(packet->packet.uin);
}

/* get parameters of the command */
static inline void* get_parameters(struct list_packet *packet)
{
	assert(packet);
	return packet->packet.params;
}

#endif
