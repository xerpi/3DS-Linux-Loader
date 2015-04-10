#!/usr/bin/python

import socket
import sys

if len(sys.argv) < 3:
    print "python client.py <ip> <file>\n"
    sys.exit(0)

s = socket.socket()
host = sys.argv[1]
port = 80
pfile = sys.argv[2]

s.connect((host, port))
f = open(pfile, "rb")
buf = f.read()
f.close();
if (f and len):
    sent = s.send(buf)
print "Sent %d bytes\n" % sent
s.close
