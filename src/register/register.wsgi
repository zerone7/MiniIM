#!/usr/bin/env python

from wsgiref.simple_server import make_server
from cgi import parse_qs, escape
import MySQLdb as mdb

html = """
<html>
<body>
<form method="post" action="register.wsgi">
<p>
nick name: <input type="text" name="nick">
</p>
<p>
password: <input type="password" name="password">
</p>
<p>
<input type="submit" value="Create account">
</p>
</form>
<p>
%s<br>
</p>
</body>
</html>
"""

def create_account(nick, password):
	if len(nick) > 31 or len(nick) < 1:
		return 0
	if len(password) > 16 or len(password) < 1:
		return 0

	try:
		conn = mdb.connect('localhost', 'im_user',
				'im_user_pass', 'user');
		cursor = conn.cursor()

		cursor.execute("SELECT MAX(uin) AS max_uin FROM user")
		max_uin = cursor.fetchone()
		uin = int(max_uin[0])
		uin += 1
		sql = "INSERT INTO user (uin, nick, password, contact_count) \
				VALUES ('%s', '%s', '%s', '%s')" \
				% (str(uin), nick, password, str(0))
		cursor.execute(sql)

		conn.commit()
		return uin
	except mdb.Error, e:
		conn.rollback()
		return 0
	except:
		return 0

def response(nick, password):
	if len(nick) > 31:
		return "your nick name is too long"
	elif len(nick) < 1:
		return " "
	if len(password) > 16:
		return "your password is too long"
	elif len(password) < 6:
		return "password is short than 6 character"
	ret = create_account(nick, password)
	if ret > 0:
		return "your uin is %u, please remember" % ret
	else:
		return "error occured, please retry"

def application(environ, start_response):
	try:
		request_body_size = int(environ.get('CONTENT_LENGTH', 0))
	except (ValueError):
		request_body_size = 0
	
	request_body = environ['wsgi.input'].read(request_body_size)
	d = parse_qs(request_body)

	nick = d.get('nick', [''])[0]
	nick = escape(nick)
	password = d.get('password', [''])[0]
	password = escape(password)

	response_body = html % (response(nick, password))
	
	status = '200 OK'

	response_headers = [('Content-Type', 'text/html'),
			('Content-Length', str(len(response_body)))]
	start_response(status, response_headers)

	return [response_body]

httpd = make_server('', 8051, application)
httpd.serve_forever()
