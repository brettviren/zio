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

    def setUp(self):
        self.snode = Node("server", self.origin)
        sport = self.snode.port("sport", zmq.SERVER)
        sport.bind()
        self.snode.online()
        self.sflow = Flow(sport)

        self.cnode = Node("client")
        cport = self.cnode.port("cport", zmq.CLIENT)
        cport.connect("server", "sport")
        self.cnode.online()
        self.cflow = Flow(cport)

    def test_conversation(self):

        # cflow is recver
        bot = Message(label='{"credit":2,"direction":"inject"}')
        self.cflow.send_bot(bot)
        bot = self.sflow.recv_bot(1000);
        assert(bot)
        assert(self.sflow.credit == 0)
        assert(self.sflow.total_credit == 2)

        # sflow is sender
        bot = Message(label='{"credit":2,"direction":"extract"}')
        self.sflow.send_bot(bot)
        bot = self.cflow.recv_bot(1000);
        assert(bot)
        assert(self.cflow.credit == 2)
        assert(self.cflow.total_credit == 2)

        self.cflow.flush_pay()
        assert(self.cflow.credit == 0)
        c = self.sflow.slurp_pay()
        assert (c==2)
        assert(self.sflow.credit == 2)

        for count in range(10):
            # note, seqno normally should sequential
            self.sflow.put(Message(coord=CoordHeader(seqno=100+count)))
            self.sflow.put(Message(coord=CoordHeader(seqno=200+count)))
            dat = self.cflow.get()
            assert(dat.seqno == 100+count)
            dat = self.cflow.get()
            assert(dat.seqno == 200+count)
        
        # normally, when a flow explicitly sends EOT the other end
        # will recv the EOT when its trying to recv another message
        # (PAY or DAT).  In this test things are synchronous and so we
        # explicitly recv_eot().
        self.cflow.send_eot(Message())

        surprise = self.sflow.recv_eot(1000)
        assert(surprise)
        self.sflow.send_eot(Message())

        expected = self.cflow.recv_eot(1000)
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
    unittest.main()
