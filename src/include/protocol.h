/*
 * 客户端发送给服务器的请求/命令
 */
#define CMD_KEEP_ALIVE		0x000A
#define CMD_LOGIN		0x0101
#define CMD_LOGOUT		0x0102
#define CMD_SET_NICK		0x0301
#define CMD_ADD_CONTACT		0x0302
#define CMD_ADD_CONTACT_REPLY	0x0401
#define CMD_CONTACT_LIST	0x0402
#define CMD_MESSAGE		0x0501
#define CMD_OFFLINE_MSG		0x0601

/*
 * 服务器发送给客户端的请求/命令
 */
#define	SRV_ERROR 		0x000E
#define SRV_LOGIN_OK		0x0101
#define SRV_SET_NICK_OK		0x0201
#define SRV_ADD_CONTACT_WAIT	0x0301
#define SRV_ADD_CONTACT_AUTH	0x0302
#define SRV_ADD_CONTACT_REPLY	0x0303
#define SRV_CONTACT_LIST	0x0401
#define SRV_CONTACT_INFO_MULTI	0x0402
#define SRV_MESSAGE		0x0501
#define SRV_OFFLINE_MSG		0x0601
#define SRV_OFFLINE_MSG_DONE	0x0602



/* 数据包结构 */
struct packet
{
	unsigned short	len;	// 数据包长度
	unsigned short	ver;	// 协议版本
	unsigned short	cmd;	// 请求/命令编号
	unsigned short	pad;	// Pad字节
	unsigned int	uin;	// 用户帐号
	void 	*params;	// 参数
};
