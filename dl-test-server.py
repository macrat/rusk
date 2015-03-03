#!/usr/bin/python

import socket
import time

LEN = 128

ssock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ssock.bind(('localhost', 8790))
ssock.listen(1)

print('listen on localhost:8790...')

try:
	while True:
		sock, _from = ssock.accept()
		print('join {0}:{1}'.format(*_from))

		try:
			sock.send(b'HTTP/1.1 200 OK\r\n')
			sock.send('Content-Length: {}\r\n'.format(LEN).encode('ascii'))
			sock.send(b'Content-Type: text/plain\r\n')
			sock.send(b'\r\n')

			for i in range(LEN):
				sock.send(str(i%10).encode('ascii'))
				print('\r{0}/{1}'.format(i+1, LEN), end='')
				time.sleep(0.1)
			print()
		except:
			pass
		finally:
			sock.close()
except KeyboardInterrupt:
	pass
finally:
	ssock.close()
