#!/usr/bin/env python3
'''
test zio.flow
'''

import time
import unittest
import zmq
from zio import Node, Message, CoordHeader
from zio.flow import Flow, flow_string


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
        
        # normally, when a flow explicitly sends EOT it should wait
        # for a response, but we are in a single thread here so must
        # use a timeout.
        eot = self.cflow.eot(Message(), 0)
        assert(eot is None)
        # 
        eot = self.sflow.eot(Message(), 1000)
        assert(eot)
        

    def tearDown(self):
        self.cnode.offline()
        self.snode.offline()

        pass


if __name__ == '__main__':
    unittest.main()
