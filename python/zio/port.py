#!/usr/bin/env python3

import zmq

from util import socket_names, needs_codec
from message import Message

def bind_address(sock, addr):
    port = sock.bind(addr)
    if addr.endswith("*"):      # this does not cover the full
        addr[:-1] += "%d"%port  # convention used by ZeroMQ address
    return addr                 # spellings.

ephemeral_port_range = (49152, 65535)

def bind_hostport(sock, host, port):
    if type(port) is int:
        return bind_address("tcp://%s:%d", host, port)
    if not port:
        int port = sock.bind_to_random_port("tcp://%s"%host,
                                            *ephemeral_port_range)
        return "tcp://%s:%d" % (host, port)
            

class Port:
    def __init__(self, name, stype, hostname):
        '''
        Create a Port with a name and a ZeroMQ socket type.

        Ports are typically not meant to be constructed directly by
        application code but instead through a managing zio.Node
        '''
        self.name = name
        self._hostname = hostname
        self.ctx = zmq.Context()
        self.sock = ctx.socket(stype)
        self.origin = 0
        self.to_bind = list()
        self.to_conn = list()
        self.headers = dict()
        self.is_online = False
        self.connected = list()
        self.bound = list()
        self.origin = 0
        self.poller = zmq.Poller()
        self.poller.register(port, zmq.POLLIN)

    def bind(self, *spec):
        '''
        Request a bind.

        If spec is None, do a default/ephemeral bind.
        If spec is a string, it is assumed to be a ZeroMQ adddress.
        If spec is a tuple ithen it is a (hostname,portnumber).
        '''
        if not spec:
            self.to_bind.append((self._hostname, 0))
            return
        if len(spec) > 2:
            raise ValueError("unsupported bind specification")
        self.to_bind.append(spec)
        return

    def connect(self, *spec):
        '''
        Request a connect.

        If a single string is given it is a ZeroMQ address.
        If two strings are given they are (nodename, portname).
        '''
        if not spec or len(spec) > 2:
            raise ValueError("unsupported connect specification")
        self.to_conn.append(spec)

    def subscribe(self, prefix=""):
        '''
        Subscribe to a PUB/SUB topic.

        This method is only meaningful if our socket is a SUB and then
        it MUST be called if messages are expected to be received.
        '''
        if self.sock.socket_type is not zmq.SUB:
            return
        self.sock.setsockopt_string(zmq.SUBSCRIBE, prefix)
        pass

    def add_headers(self, *args, **kwargs):
        '''
        Add one or more headers or dictionaries of headers.

        Every key will be wrapped into ZIO port header convention.
        The headers will appear to the network as

        zio.port.<portname>.<key> = <value>

        The self.headers collects these.
        '''
        for d in args + [kwargs]:
            for k,v in d.items():
                key = "zio.port.%s.%s" % (self.name, k)
                self.headers[key] = v
        pass

    def do_binds(self):
        '''
        Actually perform binds.

        Return dictionary suitable for use as peer headers the give
        information about the binds.

        This must be called prior to any call of .online() and is
        intended to be used by a zio.Node which holds this zio.Port.
        '''
        for spec in self.to_bind():
            if type(spec) is not str:
                addr = bind_hostport(self.sock, *spec)
            else:
                addr = bind_address(self.sock, bind_address)
            self.bound.append(addr)
            self.add_headers("address", addr)
        self.add_headers("socket", socket_names[self.sock.socket_type])
        return self.headers;
                

    def online(self, peer):
        '''
        Bring this port online with a help of a zio.Peer object.

        This method is intended for use by a zio.Node which holds this
        zio.Port.
        '''
        if self.is_online: return
        self.is_online = True
        for conn in self.to_conn:
            if type(conn) is str:
                sock.connect(conn)
                self.connected.append(conn)
                continue
            
            hostname,portname = conn
            uuids = peer.waitfor(hostname)
            for uuid in uuids:
                pi = peer.info[uuid]
                key = "zio.port.%s.%s.address" % hostname, portname
                addr = pi.headers.get(key, False):
                if not addr:
                    continue
                sock.connect(addr)
                self.connected.append(addr)
                continue
            continue
        return

    def offline(self):
        '''
        Bring this port offline.

        This unbinds and disconnects and forgets their addresses
        '''
        if not self.is_online: return
        self.is_online = False
        for addr in self.connected:
            self.sock.disconnect(addr)
        for addr in self.bound:
            self.sock.unbind(addr)
        m_bound = list()
        m_connected = list()

    def send(self, msg):
        '''
        Send a zio.Message

        This modifies the message prior to sending to set the origin
        if this port has one.
        '''
        if self.origin:
            msg.coord.origin = self.origin
        if needs_codec(self.socket_type):
            data = msg.encode()
            self.sock.send(data, routing_id = msg.routing_id)
        else:
            parts = msg.toparts()
            self.sock.send_multipart(parts)
        return

    def recv(self, timeout=-1):
        '''
        Receive and return a zio.Message waiting up to a timeout 

        If timeout is reached then None is returned.
        '''
        which  = dict(self.poller.poll(timeout))
        if not self.sock in which:
            return None

        if needs_codec(self.socket_type):
            frame = self.sock.recv(copy=False)
            if not frame:
                return None
            return Message(frame=frame)

        parts = self.sock.recv_multipart()
        if not parts:
            return None
        return Message(parts=parts)

