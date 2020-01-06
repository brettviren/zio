#!/usr/bin/env python3
'''
test zio.Message
'''

import zio
import zmq
import unittest

class TestMessage(unittest.TestCase):

    def setUp(self):
        pass

    def test_ctor_default(self):
        m = zio.Message();
        assert (m.coord.origin == 0)
        assert (m.prefix.level == zio.MessageLevel.undefined)
        assert (m.prefix.label == "")
        assert (m.prefix.form == "    ")

    def test_ctro_parts(self):
        ph = zio.PrefixHeader()
        ch = zio.CoordHeader()
        msg = zio.Message(parts=[bytes(ph), bytes(ch), b'Hello', b'World'])
        assert (2 == len(msg.payload))

    def test_ctro_encstr(self):
        msg = zio.Message(payload = 'hello world'.split())
        assert (2 == len(msg.payload))
        enc = msg.encode()

