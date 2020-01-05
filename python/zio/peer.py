#!/usr/bin/env python3
'''
ZIO peering
'''
import zmq
import pyre
import uuid
import json
from collections import namedtuple

PeerInfo = namedtuple("PeerInfo","uuid nick headers")

class Peer:
    '''
    Peer at the network to discover others and advertise self.

    This is equivalent to C++ zio::Peer and is a wrapper around Pyre,
    the Python Zyre.
    '''

    def __init__(self, nickname, **headers):
        '''
        Create a peer with a nickname.

        Extra headers may be given as a dictionary.
        '''

        self.zyre = pyre.Pyre(nickname)
        self.peers = dict() #  by UUID

        for k,v in headers.items():
            v = str(v)
            print (type(k),k,type(v),v)
            self.zyre.set_header(k,v)
        self.zyre.start()
        self.poller = zmq.Poller()
        self.poller.register(self.zyre.socket(), zmq.POLLIN)
    def __del__(self):
        self.stop()

    def stop(self):
        '''
        Stop the peer.

        This MUST be called or the application will hang.
        '''
        if hasattr(self, "zyre"):
            self.zyre.stop()
            del self.zyre

    def poll(self, timeout=0):
        '''
        Poll the network for an update.

        Return True if an event was received.  True is returned on
        reception of any type of Zyre event.  Use timeout in msec to
        wait for an event.
        '''
        which  = dict(self.poller.poll(timeout))
        if not self.zyre.socket() in which:
            return None
        msg = self.zyre.recv()
        print(msg)
        if msg[0] == b'ENTER':
            uid = uuid.UUID(bytes=msg[1])
            nick = msg[2].decode('utf-8')
            headers = json.loads(msg[3].decode('utf-8'))
            self.peers[uid] = PeerInfo(uid, nick, headers)
        if msg[0] == b'EXIT':
            uid = uuid.UUID(bytes=msg[1])
            del self.peers[uid]
            pass

        return True

    def drain(self):
        '''
        Continually poll until all queued Zyre events are processed.
        '''
        while self.poll(0):
            pass

    def matchnick(self, nick):
        '''
        Return UUIDs of all peers with matching nicks
        '''
        ret = list()
        for uid,pi in self.peers.items():
            if pi.nick == nick:
                ret.append(uid)
        return ret;
        

    def waitfor(self, nickname, timeout=-1):
        '''
        Wait for at least one peer with the nickname to be.

        Return a list of UUIDs of peers discovered to have this
        nickname.
        '''
        self.drain()
        got = self.matchnick(nickname)
        if got:
            return got
        self.poll(timeout)
        return self.matchnick(nickname)

    
