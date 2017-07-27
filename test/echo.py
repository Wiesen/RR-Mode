__author__ = 'wiesen yang'
from socket import *

TOKEN_LENGTH = 5
TOKEN_STR = "12345"

sfd = socket(AF_INET, SOCK_STREAM)

ip = '127.0.0.1'
port = 12006
sfd.connect((ip, port))

ch = ['wiesen', 'yang', 'zju', 'baidu']
for i in ch:
    message = TOKEN_STR + 'hello world!' + i + '\r\n'
    sfd.send(message)
    data = sfd.recv(100)
    print 'the data received is ',data
else:
    print 'done!'
