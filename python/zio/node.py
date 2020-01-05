#!/usr/bin/env python

from .util import guess_hostname

import zmq
from .port import Port
from .peer import Peer

class Node:
    '''
    A vertex in a ported graph with ZIO edges
    '''

    def __init__(self, nick, origin=0, hostname='127.0.0.1'):
        '''
        Create a node.

        A node has a nickname.  It may assert a unique origin number.
        A hostname may be specified for default binds of its ports or
        one will attempt to be autodetected.
        '''
        self.nick = nick
        self.origin = origin
        self._hostname = hostname or guess_hostname()
        self.ports = dict()     # by name

    def port(self, name, stype=None):
        '''
        Return a port.

        If the port with this name exists, return it.  Otherwise,
        create one with the given ZeroMQ socket type.
        '''
        if name in self.ports:
            return self.port[name]
        if stype is None:
            raise ValueError('No port "%s"' % name)
        port = Port(name, stype, self._hostname)
        port.origin = self.origin
        self.ports[name] = port
        return port

    def online(self, **headers):
        '''
        Bring node online.

        The node will advertise a number of ZIO headers based on the
        ports that have been prepared.  Additional application headers
        may also be included.
        '''
        if hasattr(self,"peer"):
            print ('Node "%s" already online' % self.nick)
            return

        for port in self.ports.values():
            hh = port.do_binds()
            headers.update(hh)
        self.peer = Peer(self.nick, **headers)
        for port in self.ports.values():
            port.online(self.peer)
        return

    def offline(self):
        '''
        Bring node offline.

        This will cause all ports to disconnect and unbind and the
        peer to be destroyed.
        '''

        for port in self.ports.values():
            port.offline()
        self.ports = dict()
        del(self.peer)
