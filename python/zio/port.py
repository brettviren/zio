#!/usr/bin/env python3

import zmq
from zmq.error import ZMQError
from zio.util import socket_names, modlog
from zio.util import clientish_recv, clientish_send
from zio.util import serverish_recv, serverish_send
from zio.message import Message

log = modlog(__name__)

def bind_address(sock, addr):
    """Bind socket to address

    Any error is printed and reraised.

    :param sock: ZeroMQ socket object
    :param addr: address in ZeroMQ format
    :returns: the address
    :rtype: string

    """
    try:
        sock.bind(addr)
    except ZMQError as e:
        print(f'failed to bind {addr}')
        raise
        
    return addr

ephemeral_port_range = (49152, 65535)

def bind_hostport(sock, host, port):
    """Bind socket TCP host and port

    :param sock: ZeroMQ socket object
    :param host: name or IP address of host to bind
    :param port: TCP port number
    :returns: ZeroMQ address string
    :rtype: string

    """
    if type(port) is int and port > 0:
        return bind_address(sock, "tcp://%s:%d" % (host, port))
    addr = "tcp://%s"%host
    try:
        #print ("bind %s:*" % addr)
        port = sock.bind_to_random_port(addr,
                                        *ephemeral_port_range)
    except ZMQError as e:
        print(f'failed to bind {addr}')
        raise
    return "tcp://%s:%d" % (host, port)
            

class Port:
    def __init__(self, name, sock, hostname='127.0.0.1'):
        '''
        Create a Port with a name and a ZeroMQ socket type.

        Ports are typically not meant to be constructed directly by
        application code but instead through a managing zio.Node
        '''
        self.name = name
        self._hostname = hostname
        if type(sock) is int:
            self.ctx = zmq.Context()
            self.sock = self.ctx.socket(sock)
        else:
            self.sock = sock
        self.origin = None
        self.to_bind = list()
        self.to_conn = list()
        self.headers = dict()
        self.is_online = False
        self.connected = list()
        self.bound = list()
        self.poller = zmq.Poller()
        self.poller.register(self.sock, zmq.POLLIN)

    def __str__(self):
        return "[port %s]: type:%s binds:%d(todo:%d) conns:%d(todo:%d)" % \
            (self.name, socket_names[self.sock.type],
             len(self.bound), len(self.to_bind),
             len(self.connected), len(self.to_conn))

    def bind(self, *spec):
        '''
        Request a bind.

        If spec is None, do a default/ephemeral bind.
        If spec is a string, it is assumed to be a ZeroMQ adddress.
        If spec is a tuple ithen it is a (hostname,portnumber).
        '''
        # log.debug(f'bind({spec})')
        if not spec:
            log.debug (f'bind port "{self.name}" to default {self._hostname}')
            self.to_bind.append((self._hostname, 0))
            return
        if len(spec) > 2:
            raise ValueError("unsupported bind specification")
        if len(spec) == 1:
            if spec[0].startswith("inproc://") and self.sock.type in (zmq.CLIENT,zmq.SERVER):
                raise ValueError('CLIENT and SERVER does not work with inproc://')
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
        if self.sock.type is not zmq.SUB:
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
        for d in list(args) + [kwargs]:
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
        for spec in self.to_bind:
            if len(spec) == 2:
                #print ("[port %s] bind host port %s" % (self.name, spec,))
                addr = bind_hostport(self.sock, *spec)
            else:               # direct
                #print ("[port %s] bind address %s" % (self.name, spec[0],))
                addr = bind_address(self.sock, spec[0])
            self.bound.append(addr)
            self.add_headers(address=addr)
        self.to_bind = list()
        self.add_headers(socket=socket_names[self.sock.type])
        return self.headers;
                

    def online(self, peer = None):
        '''
        Bring this port online.

        If no peer is given then indirect connects will fail.

        This method is intended for use by a zio.Node which holds this
        zio.Port.
        '''
        if self.is_online: return
        self.is_online = True
        for conn in self.to_conn:
            if len(conn) == 1 and type(conn[0]) is str:
                conn = conn[0]
                self.sock.connect(conn)
                self.connected.append(conn)
                continue
            
            if not peer:
                err="[port %s]: no peer given, can not connect %s" % \
                    (self.name, conn)
                raise ValueError(err)
                      
            nodename,portname = conn
            uuids = peer.waitfor(nodename)
            if not uuids:
                raise RuntimeError('failed to wait for "%s"' % nodename)
            for uuid in uuids:
                pi = peer.peers[uuid]
                key = f"zio.port.{portname}.address"
                addr = pi.headers.get(key, False)
                if not addr:
                    continue
                self.sock.connect(addr)
                self.connected.append(addr)
                continue
            continue
        self.to_conn = list()
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
        if self.origin is not None:
            msg.coord.origin = self.origin

        if self.sock.type in (zmq.SERVER, zmq.ROUTER):
            log.debug(f'port [{self.name}] send {msg} rid:{msg.routing_id}')
            return serverish_send(self.sock, msg.routing_id, msg.toparts())
        if self.sock.type in (zmq.CLIENT, zmq.DEALER):
            log.debug(f'port [{self.name}] send {msg}')
            return clientish_send(self.sock, msg.toparts())

        log.debug(f'port [{self.name}] send {msg}')
        return self.sock.send_multipart(msg.toparts())


    def recv(self, timeout=None):
        '''
        Receive and return a zio.Message waiting up to a timeout 

        If timeout is reached then None is returned.
        '''
        which  = dict(self.poller.poll(timeout))
        if not self.sock in which:
            return None         # timeout

        if self.sock.type in (zmq.SERVER, zmq.ROUTER):
            rid, parts = serverish_recv(self.sock)
            msg = Message(routing_id = rid, parts = parts)
            log.debug(f'port [{self.name}] recv {msg} rid:{msg.routing_id}')
            return msg
            
        if self.sock.type in (zmq.CLIENT, zmq.DEALER):
            parts = clientish_recv(self.sock)
            msg = Message(parts = parts)
            log.debug(f'port [{self.name}] recv {msg}')
            return msg

        parts = sock.recv_multipart()
        msg = Message(parts = parts)
        log.debug(f'port [{self.name}] recv {msg}')
        return msg
        

