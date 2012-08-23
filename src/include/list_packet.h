#ifndef _LIST_PACKET_H_
#define _LIST_PACKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "list.h"

#define LIST_PACKET_SIZE	(sizeof(struct list_head) + MAX_PACKET_LEN)

#define char_ptr(ptr)		((char *)(ptr))
#define get_field(ptr, offset, type)			\
	(*((type *)((char_ptr(ptr) + (offset)))))
#define get_field_uint16_t(ptr, offset)			\
	get_field(ptr, offset, uint16_t)
#define get_field_uint32_t(ptr, offset)			\
	get_field(ptr, offset, uint32_t)
#define get_field_ptr(ptr, offset)			\
	(char_ptr(ptr) + (offset))

#define set_field(ptr, offset, value, type)		\
	*((type *)(char_ptr(ptr) + (offset))) = (value)
#define set_field_uint16_t(ptr, offset, value)		\
	set_field(ptr, offset, value, uint16_t)
#define set_field_uint32_t(ptr, offset, value)		\
	set_field(ptr, offset, value, uint32_t)
#define set_field_ptr(ptr, offset, val_ptr, size)	\
	memcpy(get_field_ptr(ptr, offset), (val_ptr), (size))

/* packet list */
struct list_packet {
	struct list_head list;
	struct packet packet;
};

/* get length of the packet */
static inline uint16_t get_length(struct list_packet *lp)
{
	return lp->packet.len;
}

/* set length of the packet */
static inline void set_length(struct list_packet *lp, uint16_t length)
{
	lp->packet.len = length;
}

/* get version of the packet */
static inline uint16_t get_version(struct list_packet *lp)
{
	return lp->packet.ver;
}

/* set version of the packet */
static inline void set_version(struct list_packet *lp, uint16_t version)
{
	lp->packet.ver = version;
}

/* get command of the packet */
static inline uint16_t get_command(struct list_packet *lp)
{
	return lp->packet.cmd;
}

/* set command of the packet */
static inline void set_command(struct list_packet *lp, uint16_t command)
{
	lp->packet.cmd = command;
}

/* get uin of the packet */
static inline uint32_t get_uin(struct list_packet *lp)
{
	return lp->packet.uin;
}

/* set uin of the packet */
static inline void set_uin(struct list_packet *lp, uint32_t uin)
{
	lp->packet.uin = uin;
}

/* get pointer of parameters of the packet */
static inline char* get_parameters(struct list_packet *lp)
{
	return lp->packet.params;
}

#define __CL_PASS_LENGTH_OFFSET		0
#define __CL_PASSWORD_OFFSET		2

/* set password field in CMD_LOGIN packet */
static inline void cl_set_password(struct list_packet *lp,
		const char *pass, uint16_t length)
{
	set_field_uint16_t(get_parameters(lp), __CL_PASS_LENGTH_OFFSET,
			length);
	set_field_ptr(get_parameters(lp), __CL_PASSWORD_OFFSET,
			pass, length);
}

#define __CSN_NICK_LENGTH_OFFSET	0
#define __CSN_NICK_OFFSET		2

/* set nick field in CMD_SET_NICK packet */
static inline void csn_set_nick(struct list_packet *lp,
		const char *nick, uint16_t length)
{
	set_field_uint16_t(get_parameters(lp), __CSN_NICK_LENGTH_OFFSET,
			length + 1);
	set_field_ptr(get_parameters(lp), __CSN_NICK_OFFSET,
			nick, length + 1);
	set_field(get_parameters(lp), __CSN_NICK_OFFSET + length,
			'\0', char);
}

#define __CAC_UIN_OFFSET		0

/* set uin field in CMD_ADD_CONTACT packet */
static inline void cac_set_uin(struct list_packet *lp, uint32_t uin)
{
	set_field_uint32_t(get_parameters(lp), __CAC_UIN_OFFSET, uin);
}

#define __CACR_TO_UIN_OFFSET		0
#define __CACR_REPLY_TYPE_OFFSET	4

/* set to_uin field in CMD_ADD_CONTACT_REPLY packet */
static inline void cacr_set_to_uin(struct list_packet *lp, uint32_t to_uin)
{
	set_field_uint32_t(get_parameters(lp), __CACR_TO_UIN_OFFSET,
			to_uin);
}

/* set reply_type field in CMD_ADD_CONTACT_REPLY packet */
static inline void cacr_set_reply_type(struct list_packet *lp,
		uint16_t reply_type)
{
	set_field_uint16_t(get_parameters(lp), __CACR_REPLY_TYPE_OFFSET,
			reply_type);
}

#define __CCIM_COUNT_OFFSET		0
#define __CCIM_UINS_OFFSET		2

/* set uins field in CMD_CONTACT_INFO_MULTI packet */
static inline void ccim_set_uins(struct list_packet *lp,
		uint32_t *uins, uint16_t count)
{
	set_field_uint16_t(get_parameters(lp), __CCIM_COUNT_OFFSET,
			count);
	set_field_ptr(get_parameters(lp), __CCIM_UINS_OFFSET,
			uins, count * 4);
}

#define __CM_TO_UIN_OFFSET		0
#define __CM_MSG_LENGTH_OFFSET		8
#define __CM_MESSAGE_OFFSET		10

/* set to_uin field in CMD_MESSAGE packet */
static inline void cm_set_to_uin(struct list_packet *lp, uint32_t to_uin)
{
	set_field_uint32_t(get_parameters(lp), __CM_TO_UIN_OFFSET,
			to_uin);
}

/* set message field in CMD_MESSAGE packet */
static inline void cm_set_message(struct list_packet *lp,
		const char *message, uint16_t length)
{
	set_field_uint16_t(get_parameters(lp), __CM_MSG_LENGTH_OFFSET,
			length + 1);
	set_field_ptr(get_parameters(lp), __CM_MESSAGE_OFFSET,
			message, length + 1);
	set_field(get_parameters(lp), __CM_MESSAGE_OFFSET + length,
			'\0', char);
}

#define __SE_CLIENT_CMD_OFFSET		0
#define __SE_ERROR_TYPE_OFFSET		2

/* get CLIENT_CMD field in SRV_ERROR packet */
static inline uint16_t se_get_client_cmd(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp), __SE_CLIENT_CMD_OFFSET);
}

/* set CLIENT_CMD field in SRV_ERROR packet */
static inline void se_set_client_cmd(struct list_packet *lp,
		uint16_t client_cmd)
{
	set_field_uint16_t(get_parameters(lp), __SE_CLIENT_CMD_OFFSET,
			client_cmd);
}

/* get ERROR_TYPE field in SRV_ERROR packet */
static inline uint16_t se_get_error_type(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp), __SE_ERROR_TYPE_OFFSET);
}

/* set ERROR_TYPE field in SRV_ERROR packet */
static inline void se_set_error_type(struct list_packet *lp,
		uint16_t error_type)
{
	set_field_uint16_t(get_parameters(lp), __SE_ERROR_TYPE_OFFSET,
			error_type);
}

#define __SLO_NICK_LENGTH_OFFSET	0
#define __SLO_NICK_OFFSET		2

/* get NICK_LENGTH field in SRV_LOGIN_OK packet */
static inline uint16_t slo_get_nick_length(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SLO_NICK_LENGTH_OFFSET);
}

/* get NICK field in SRV_LOGIN_OK packet */
static inline char* slo_get_nick(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp), __SLO_NICK_OFFSET);
}

#define __SSNO_NICK_LENGTH_OFFSET	0
#define __SSNO_NICK_OFFSET		2

/* get NICK_LENGTH field in SRV_SET_NICK_OK packet */
static inline uint16_t ssno_get_nick_length(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SSNO_NICK_LENGTH_OFFSET);
}

/* get nick field in SRV_SET_NICK_OK packet */
static inline char* ssno_get_nick(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp), __SSNO_NICK_OFFSET);
}

#define __SACW_FROM_UIN_OFFSET		0
#define __SACW_NICK_LENGTH_OFFSET	4
#define __SACW_NICK_OFFSET		6

/* get FROM_UIN field in SRV_ADD_CONTACT_WAIT packet */
static inline uint32_t sacw_get_from_uin(struct list_packet *lp)
{
	return get_field_uint32_t(get_parameters(lp), __SACW_FROM_UIN_OFFSET);
}

/* get NICK_LENGTH field in SRV_ADD_CONTACT_WAIT packet */
static inline uint16_t sacw_get_nick_length(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SACW_NICK_LENGTH_OFFSET);
}

/* get NICK field in SRV_ADD_CONTACT_WAIT packet */
static inline char* sacw_get_nick(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp), __SACW_NICK_OFFSET);
}

#define __SCL_CONTACT_COUNT_OFFSET	0
#define __SCL_CONTACT_UINS_OFFSET	2

/* get CONTACT_COUNT in SRV_CONTACT_LIST packet */
static inline uint16_t scl_get_contact_count(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SCL_CONTACT_COUNT_OFFSET);
}

/* get CONTACT_UINS field in SRV_CONTACT_LIST packet */
static inline uint32_t* scl_get_contact_uins(struct list_packet *lp)
{
	return (uint32_t *)get_field_ptr(get_parameters(lp),
			__SCL_CONTACT_UINS_OFFSET);
}

#define __SCIM_CONTACT_COUNT_OFFSET	0
#define __SCIM_CONTACT_START_OFFSET	2
#define __SCIM_CONTACT_UIN_OFFSET	0
#define __SCIM_CONTACT_STATUS_OFFSET	4
#define __SCIM_CONTACT_NICK_LEN_OFFSET	6
#define __SCIM_CONTACT_NICK_OFFSET	8

/* get CONTACT_COUNT field in SRV_CONTACT_INFO_MULTI packet */
static inline uint16_t scim_get_contact_count(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SCL_CONTACT_COUNT_OFFSET);
}

/* get first pointer of UIN in SRV_CONTACT_INFO_MULTI packet */
static inline char* scim_get_first_ptr(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp),
			__SCIM_CONTACT_START_OFFSET);
}

/* get uin of current contact in SRV_CONTACT_INFO_MULTI packet */
static inline uint32_t scim_get_uin(char *current)
{
	return get_field_uint32_t(current, __SCIM_CONTACT_UIN_OFFSET);
}

/* get status of current contact in SRV_CONTACT_INFO_MULTI packet */
static inline uint16_t scim_get_status(char *current)
{
	return get_field_uint16_t(current, __SCIM_CONTACT_STATUS_OFFSET);
}

/* get nick_length of current contact in SRV_CONTACT_INFO_MULTI packet */
static inline uint16_t scim_get_nick_length(char *current)
{
	return get_field_uint16_t(current, __SCIM_CONTACT_NICK_LEN_OFFSET);
}

/* get nick of current contact is SRV_CONTACT_INFO_MULTI packet */
static inline char* scim_get_nick(char *current)
{
	return get_field_ptr(current, __SCIM_CONTACT_NICK_OFFSET);
}

/* get next pointer of UIN in SRV_CONTACT_INFO_MULTI packet */
static inline char* scim_get_next_ptr(char *current)
{
	int nick_length = scim_get_nick_length(current);
	return get_field_ptr(current,
			__SCIM_CONTACT_NICK_OFFSET + nick_length);
}

#define __SM_FROM_UIN_OFFSET	0
#define __SM_TYPE_OFFSET	8
#define __SM_MSG_LENGTH_OFFSET	10
#define __SM_MESSAGE_OFFSET	12

/* get FROM_UIN field in SRV_MESSAGE packet */
static inline uint32_t sm_get_from_uin(struct list_packet *lp)
{
	return get_field_uint32_t(get_parameters(lp), __SM_FROM_UIN_OFFSET);
}

/* get TYPE field in SRV_MESSAGE packet */
static inline uint16_t sm_get_type(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp), __SM_TYPE_OFFSET);
}

/* get MSG_LENGTH field in SRV_MESSAGE packet */
static inline uint16_t sm_get_msg_length(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp), __SM_MSG_LENGTH_OFFSET);
}

/* get MESSAGE field in SRV_MESSAGE packet */
static inline char* sm_get_message(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp), __SM_MESSAGE_OFFSET);
}

#define __SOM_MSG_COUNT_OFFSET	0
#define __SOM_START_OFFSET	2
#define __SOM_FROM_UIN_OFFSET	0
#define __SOM_TYPE_OFFSET	8
#define __SOM_MSG_LEGNTH_OFFSET	10
#define __SOM_MESSAGE_OFFSET	12

/* get MSG_COUNT field in SRV_OFFLINE_MSG packet */
static inline uint16_t som_get_msg_count(struct list_packet *lp)
{
	return get_field_uint16_t(get_parameters(lp),
			__SOM_MSG_COUNT_OFFSET);
}

/* get first message in SRV_OFFLINE_MSG packet */
static inline char* som_get_first_ptr(struct list_packet *lp)
{
	return get_field_ptr(get_parameters(lp),
			__SOM_START_OFFSET);
}

/* get FROM_UIN field in SRV_OFFLINE_MSG packet */
static inline uint32_t som_get_from_uin(char *current)
{
	return get_field_uint32_t(current, __SOM_FROM_UIN_OFFSET);
}

/* get TYPE field in SRV_OFFLINE_MSG packet */
static inline uint16_t som_get_type(char *current)
{
	return get_field_uint16_t(current, __SOM_TYPE_OFFSET);
}

/* get LENGTH field in SRV_OFFLINE_MSG packet */
static inline uint16_t som_get_msg_length(char *current)
{
	return get_field_uint16_t(current, __SOM_MSG_LEGNTH_OFFSET);
}

/* get MESSAGE field in SRV_OFFLINE_MSG packet */
static inline char* som_get_message(char *current)
{
	return get_field_ptr(current, __SOM_MESSAGE_OFFSET);
}

/* get next message in SRV_OFFLINE_MSG packet */
static inline char* som_get_next_ptr(char *current)
{
	int msg_length = som_get_msg_length(current);
	return get_field_ptr(current,
			__SOM_MESSAGE_OFFSET + msg_length);
}

#endif
