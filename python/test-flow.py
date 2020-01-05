#!/usr/bin/env python3
'''
test zio.flow
'''

import zio

snode = zio.Node("server", 42)
sport = snode.port("sport", zio.SERVER)
sport.bind()
snode.online()

cnode = zio.Node("client")
cport = cnode.port("cport", zio.CLIENT)
cport.connect("server", "sport")
cnode.online()


cport.sock.send_string("hello")
s = sport.sock.recv_string()
assert(s == "hello")

snode.offline()
cnode.offline()
