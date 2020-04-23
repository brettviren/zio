#!/usr/bin/env python3
'''
test zio.Node
'''

import zio
import zmq
import time

import unittest

from zio.util import mainlog

class TestNode(unittest.TestCase):

    origin = 42

    def setUp(self):
        self.snode = zio.Node("server", self.origin)
        sport = self.snode.port("sport", zmq.SERVER)
        sport.bind()
        self.snode.online()

        self.cnode = zio.Node("client")
        cport = self.cnode.port("cport", zmq.CLIENT)
        cport.connect("server", "sport")
        self.cnode.online()

    def test_noport(self):
        try:
            p = self.snode.port("no such port")
        except KeyError:
            return
        raise RuntimeError("zio.Node should raise error on unknown port")

    def test_pingpong(self):
        sport = self.snode.port("sport")
        cport = self.cnode.port("cport")

        msg = zio.Message(form="TEXT", label="This is a message to you, Rudy",
                          payload=["Stop your messing around",
                                   "Better think of your future"])
        cport.send(msg);
        msg2 = sport.recv(timeout=1000)
        msg2.payload=['Time you straighten right out',
                      "Else you'll wind up in jail"]
        assert (type(msg2.payload[0]) == bytes)
        #print("sending to rid %d" % msg2.routing_id)
        sport.send(msg2)        # should forward the routing_id
        msg3 = cport.recv(timeout=1000)
        assert (msg2.payload[0] == msg3.payload[0])
        
    def tearDown(self):
        self.snode.offline()
        self.cnode.offline()        

if __name__ == '__main__':
    mainlog()
    unittest.main()
    
