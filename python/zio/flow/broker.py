#!/usr/bin/env python3
'''
ZIO brokers bring together ZIO clients.

'''

import json
from collections import namedtuple
import zmq
import zio
from .util import objectify, switch_direction
import logging
log = logging.getLogger("zpb")

class Broker:
    def __init__(self, server, factory):
        '''Create a flow broker.

        Parameters
        ----------
        server : zio.Port
            An online SERVER port
        factory : callable
            See below
        
        This broker routes messages between a pair of clients: a
        remote client and its handler.  It does this by adding a flow
        initiation protocol to ZIO flow protocol.

        On receipt of a BOT, the broker passes it to the factory which
        should return True if the BOT was accepted else None.  The
        factory call should return as promptly as possible as the
        broker blocks.

        If factory returns True then it is expected that the factory
        has started some other client which contacs the broker with
        the BOT.  The factory or the new client may modify the BOT as
        per flow protorocl but must leave intact the `cid` attribute
        placed in the flow object by the broker.

        Note that the flow handler protocol does not communicate the
        location of the broker's SERVER socket.  It is up to handler
        implementations to locate the the server.

        '''
        self.server = server
        self.factory = factory
        self.other = dict()     # map router IDs
        self.handlers = set()

    def poll(self, timeout=None):
        '''
        Poll for at most one message from the SERVER port

        Raises exceptions.

        '''
        msg = self.server.recv(timeout)
        if not msg:
            log.error (f'broker poll timeout with {timeout}')
            raise TimeoutError(f'broker poll timeout with {timeout}')
        log.debug (f'broker recv {msg}')
        rid = msg.routing_id

        orid = self.other.get(rid, None)
        if orid:                # we have its other
            if orid in self.handlers:
                log.debug (f"broker route c2h {rid} -> {orid}:\n{msg}\n")
            else:
                log.debug (f"broker route h2c {rid} -> {orid}:\n{msg}\n")
            msg.routing_id = orid
            self.server.send(msg)
            return

        # message is from new client or handler 

        fobj = objectify(msg)
        ftype = fobj.get("flow", None)
        if not ftype:
            raise TypeError(f'message not type FLOW')
        if ftype != "BOT":
            raise TypeError(f'flow message not type BOT')


        cid = fobj.get('cid',None)
        if cid:             # BOT from handler
            self.handlers.add(rid)
            self.other[rid] = cid
            self.other[cid] = rid
            del fobj["cid"]
            log.debug(f'broker route from handler {rid} <--> {cid}')

            label_for_client = json.dumps(fobj)

            fobj = switch_direction(fobj)                
            msg.label = json.dumps(fobj)                
            msg.routing_id = rid
            log.debug (f"broker route c2h {cid} -> {rid}:\n{msg}\n")
            self.server.send(msg) # to handler

            msg.label = label_for_client
            msg.routing_id = cid
            log.debug (f"broker route h2c {rid} -> {cid}:\n{msg}\n")
            self.server.send(msg)
            return

        # BOT from client
        log.debug(f'broker route from client {rid}')
        fobj = switch_direction(fobj)
            
        fobj["cid"] = rid
        msg.label = json.dumps(fobj)
        msg.routing_id = 0
        got = self.factory(msg)
        if got:
            return

        # factory refuses
        log.debug (f"broker factory rejects {msg.label}")
        fobj['flow'] = 'EOT'
        msg.label = json.dumps(fobj)
        msg.routing_id = rid
        self.server.send(msg)
        raise RuntimeError('broker factory rejects {msg.label}')


    def stop(self):
        self.factory.stop()
        return



# fixme: broker doesn't handle endpoints disappearing.


