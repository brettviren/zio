#!/usr/bin/env python3
'''
test zio.flow
'''

import time
import unittest
import zmq
from zio import Node, Message, CoordHeader
from zio.flow import Flow
from zio.util import modlog, mainlog, stringify, objectify

log = modlog('test_flow')

class TestFlow(unittest.TestCase):

    origin = 42
    credit = 2

    def setUp(self):
        self.snode = Node("server", self.origin)
        sport = self.snode.port("sport", zmq.SERVER)
        sport.bind()
        self.snode.online()
        self.sflow = Flow(sport, "extract", TestFlow.credit)
        assert(self.sflow.sm.is_giver())

        self.cnode = Node("client")
        cport = self.cnode.port("cport", zmq.CLIENT)
        cport.connect("server", "sport")
        self.cnode.online()
        self.cflow = Flow(cport, "inject", TestFlow.credit)
        assert(self.cflow.sm.is_taker())

    def test_conversation(self):

        # normally, we use .bot() but here we are synchronous with
        # both endpoints so have to break up the steps of at least one
        # endpoint.  
        self.cflow.send_bot()

        # this can pretend to be async
        sbot = self.sflow.bot()
        assert(sbot)
        assert(sbot.form == 'FLOW')

        cbot = self.cflow.recv()
        assert(cbot)
        assert(cbot.form == 'FLOW')

        # here, server is giver, should start with no credit
        assert(self.sflow.credit == 0)
        assert(self.sflow.total_credit == TestFlow.credit)
        # here, client is taker, should start with all credit
        assert(self.cflow.credit == TestFlow.credit)
        assert(self.cflow.total_credit == TestFlow.credit)

        log.debug("flow BOT handshake done")
        assert(self.cflow.sm.state == "READY");
        assert(self.sflow.sm.state == "READY");

        # this also imitates PAY 
        self.cflow.begin()
        log.debug("client flow began")
        assert(self.cflow.sm.state == "taking_HANDSOUT");

        self.sflow.begin()
        log.debug("server flow began")
        assert(self.sflow.sm.state == "giving_GENEROUS");


        send_eot = False
        for count in range(10):
            log.debug(f"test_flow: server put in {self.sflow.sm.state}")
            dat = Message(form='FLOW')
            self.sflow.put(dat)
            log.debug(f"test_flow: client get in {self.cflow.sm.state}")
            dat = self.cflow.get()
            # flow protocol: BOT=0, DAT=1+
            assert(dat.seqno == 1+count)

        

        # normally, when a flow explicitly sends EOT the other end
        # will recv the EOT when its trying to recv another message
        # (PAY or DAT). 
        self.cflow.eotsend()

        should_be_eot = self.sflow.recv()

        assert(should_be_eot)
        self.sflow.eotsend()
        expected = self.cflow.eotrecv()
        assert(expected)


    # def test_flow_string(self):
    #     msg = Message(label='{"extra":42}')
    #     msg.label = stringify('DAT', **objectify(msg))
    #     fobj = objectify(msg)
    #     assert(fobj["extra"] == 42)
    #     assert(fobj["flow"] == "DAT")



    def tearDown(self):
        self.cnode.offline()
        self.snode.offline()

        pass


if __name__ == '__main__':
    mainlog()
    unittest.main()
