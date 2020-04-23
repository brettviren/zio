#!/usr/bin/env python3
'''
test zio.port
'''

import time
import unittest
import zmq
from zio import Port, Node, Message
from zio.util import modlog, mainlog

log = modlog('test_port')


class TestPort(unittest.TestCase):

    origin = 42

    def setUp(self):
        self.snode = Node("server", self.origin)
        sport = self.snode.port("sport", zmq.SERVER)
        sport.bind()
        self.snode.online()

        self.cnode = Node("client")
        cport = self.cnode.port("cport", zmq.CLIENT)
        cport.connect("server", "sport")
        self.cnode.online()

    def tearDown(self):
        pass

    def test_sendrecv(self):
        sport = self.snode.port("sport")
        cport = self.cnode.port("cport")

        lobj = dict(foo='bar')
        msg = Message(form='TEST',label_object=lobj)
        log.debug(f'{msg}')
        cport.send(msg)
        log.debug('now recv')
        msg2 = sport.recv()
        assert(msg2)
        assert(msg2.form == 'TEST')
        lobj2 = msg2.label_object
        assert(lobj2)
        assert(lobj2 == lobj)

if __name__ == '__main__':
    mainlog()
    unittest.main()
        
