#!/usr/bin/env python

import struct
import socket
import ctypes

class Client:
	def __init__(self, fd):
		self.fd = fd
		self.uin = 100
	# create the common 12 bytes header, include length, command, uin, etc.
	def createHeader(self, length, command):
		buff = ctypes.create_string_buffer(length)
		version = 0x01
		pad = 0x00
		values = (length, version, command, pad, self.uin)
		struct.pack_into('! 4H I', buff, 0, *values)
		return buff
	# send login packet to server
	def login(self, uin, password):
		self.uin = uin
		offset = 12
		length = offset + 2 + len(password)
		command = 0x0101
		buff = self.createHeader(length, command)
		string = '! H {0}s'.format(len(password))
		struct.pack_into(string, buff, offset, 0x02, password)
		self.fd.sendall(buff)
	# send set nick packet to server
	def set_nick(self, nick):
		offset = 12
		length = offset + 2 + len(nick) + 1
		command = 0x0201
		buff = self.createHeader(length, command)
		string = '! H {0}s s'.format(len(nick))
		struct.pack_into(string, buff, offset, 0x02, nick, '\0')
		self.fd.sendall(buff)
	# send add contact packet to server
	def add_contact(self, uin):
		offset = 12
		length = offset + 4
		command = 0x0301
		buff = self.createHeader(length, command)
		string = '! I'
		struct.pack_into(string, buff, offset, uin)
		self.fd.sendall(buff)

if __name__ == '__main__':
	host = '127.0.0.1'
	port = 11001
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect((host, port))
	client = Client(s)
	cmd = raw_input('please enter your command: ')
	while cmd != 'quit':
		cmd_list = cmd.split(' ')
		if len(cmd_list) == 0:
			print 'bad command'
		elif cmd_list[0] == 'login':
			uin = 101
			password = '123456'
			if len(cmd_list) > 1:
				uin = int(cmd_list[1])
			if len(cmd_list) > 2:
				password = cmd_list[2]
			client.login(uin, password)
		elif cmd_list[0] == 'set_nick':
			nick = 'new nick'
			if len(cmd_list) > 1:
				nick = cmd_list[1]
			client.set_nick(nick)
		elif cmd_list[0] == 'add_contact':
			contact = 102
			if len(cmd_list) > 1:
				contact = int(cmd_list[1])
			client.add_contact(contact)
		else:
			print 'bad command'
		cmd = raw_input('please enter your command: ')
