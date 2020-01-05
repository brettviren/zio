#!/usr/bin/env python3
'''
ZIO peering
'''

class Peer:
    '''
    Peer at the network to discover others and advertise self.

    This is equivalent to C++ zio::Peer and is a wrapper around Pyre,
    the Python Zyre.
    '''

    def __init__(self, nickname, headers=None, verbose=False):
        '''
        Create a peer with a nickname.

        Extra headers may be given as a dictionary.
        '''

        self.zyre = pyre.Pyre(nickname)
        self.headers = headers or dict()
        self.peers = dict()
        ...
        
    def poll(self, timeout=0):
        '''
        Poll the network for an update.

        Return True if an event was received.  True is returned on
        reception of any type of Zyre event.  Use timeout in msec to
        wait for an event.
        '''
        ...
        pass

    def drain(self):
        '''
        Continually poll until all queued Zyre events are processed.
        '''

    def waitfor(self, nickname, timeout=-1):
        '''
        Wait for at least one peer with the nickname to be.

        Return a list of UUIDs of peers discovered to have this
        nickname.
        '''
        ...
        return

    
