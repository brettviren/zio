#!/usr/bin/env python3

import logging
logger = logging.getLogger("zio.peer")
logger.level = logging.DEBUG

import zio
import unittest

class TestPeer(unittest.TestCase):

    nick1 = "zio_test_peer_nick1"
    nick2 = "zio_test_peer_nick2"

    def setUp(self):
        self.p1 = zio.Peer(self.nick1, a='42', b='hello') #, verbose=True)
        self.p2 = zio.Peer(self.nick2, a='6.9', b='world') #, verbose=True)

    def tearDown(self):
        del self.p2
        del self.p1

    def test_01poll(self):
        ok = self.p2.poll(timeout=1000)
        assert(ok)

    def test_02waitfor(self):
        ok = self.p1.waitfor(self.nick2, timeout=1000)
        assert(ok)
        uids = self.p1.matchnick(self.nick2)
        assert (len(uids)>0)
        pi = self.p1.peers[uids[0]]
        assert(pi.nick == self.nick2)
        assert(pi.headers['b'] == 'world') # may fail if other peers match nick

    def test_03drain(self):
        self.p1.drain()
        self.p2.drain()

    def test_04stop(self):
        self.p1.stop()

if __name__ == '__main__':
    unittest.main()
    
