#!/usr/bin/env python3
'''
test zio.flow
'''

import time
import unittest
import zmq
from zio import Node, Message, CoordHeader
from zio.flow import Flow, stringify, objectify

class TestFlow(unittest.TestCase):

    origin = 42
    credit = 2

    def setUp(self):
        self.snode = Node("server", self.origin)
        sport = self.snode.port("sport", zmq.SERVER)
        sport.bind()
        self.snode.online()
        self.sflow = Flow(sport, "extract", TestFlow.credit)

        self.cnode = Node("client")
        cport = self.cnode.port("cport", zmq.CLIENT)
        cport.connect("server", "sport")
        self.cnode.online()
        self.cflow = Flow(cport, "inject", TestFlow.credit)

    def test_conversation(self):

        self.cflow.send_bot()

        sbot = self.sflow.bot()
        assert(sbot)
        assert(sflow.credit == 0)
        assert(sflow.total_credit == TestFlow.credit)

        cbot = self.recv()
        assert(cbot)
        assert(self.cflow.credit == 2)
        assert(self.cflow.total_credit == 2)

        self.cflow.flush_pay()

        for count in range(10):
            self.sflow.put(Message())
            dat = self.cflow.get()
            # flow protocol: BOT=0, DAT=1+
            assert(dat.seqno == 1+count)

        
        # normally, when a flow explicitly sends EOT the other end
        # will recv the EOT when its trying to recv another message
        # (PAY or DAT).  In this test things are synchronous and so we
        # explicitly recv_eot().
        self.cflow.eotsend()

        surprise = self.sflow.recv()
        assert(surprise)
        self.sflow.sendeot()
        expected = self.cflow.eotrecv()
        assert(expected)
        

    def test_flow_string(self):
        msg = Message(label='{"extra":42}')
        msg.label = stringify('DAT', **objectify(msg))
        fobj = objectify(msg)
        assert(fobj["extra"] == 42)
        assert(fobj["flow"] == "DAT")



    def tearDown(self):
        self.cnode.offline()
        self.snode.offline()

        pass


if __name__ == '__main__':
    import logging
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
