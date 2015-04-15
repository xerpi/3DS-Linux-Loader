#!/usr/bin/python

import socket
import sys

if len(sys.argv) < 3:
    print "python client.py <ip> <file>\n"
    sys.exit(0)

port = 80
host = sys.argv[1]
pfile = sys.argv[2]

s = socket.socket()
s.connect((host, port))
f = open(pfile, "rb")
buf = f.read()
f.close();
if (f and len(buf)):
    sent = s.send(buf)
print "Sent %d bytes\n" % sent
s.close
