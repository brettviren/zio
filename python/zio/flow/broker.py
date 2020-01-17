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
log = logging.getLogger(__name__)

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

    def poll(self, timeout=None):
        '''
        Poll for at most one message from the SERVER port

        Return None if timeout, False if error, True if nominal

        '''
        msg = self.server.recv(timeout)
        if not msg:
            log.debug (f'broker poll timeout with {timeout}')
            return None
        #log.debug ("broker poll:",msg)
        rid = msg.routing_id
        fobj = objectify(msg)
        ftype = fobj.get("flow", None)
        if not ftype:
            log.debug (f'broker poll got non flow msg:\n{msg}')
            return False

        # Special, assymetric handling of BOT
        if ftype == "BOT":
            cid = fobj.get('cid',None)
            if cid:             # BOT from handler
                self.other[rid] = cid
                self.other[cid] = rid
                del fobj["cid"]

                label_for_client = json.dumps(fobj)

                fobj = switch_direction(fobj)                
                msg.label = json.dumps(fobj)                
                msg.routing_id = rid
                self.server.send(msg) # to handler

                msg.label = label_for_client
                # fall through to send to client
                
            else:               # BOT from client
                fobj = switch_direction(fobj)
                if fobj is None:
                    return False 
                fobj["cid"] = rid
                msg.label = json.dumps(fobj)
                msg.routing_id = 0
                got = self.factory(msg)
                if got is None:
                    fobj['flow'] = 'EOT'
                    msg.label = json.dumps(fobj)
                    msg.routing_id = rid
                    self.server.send(msg)
                    return False
                else:
                    return True
                return False    # unknown respose 

        # route message to other
        cid = self.other[rid]
        msg.routing_id = cid
        log.debug (f"broker route {rid} -> {cid}:\n{msg}\n")
        self.server.send(msg)

        # if EOT comes through, we should forget the routing
        if ftype == 'EOT':
            del self.other[rid]

        return True

    def stop(self):
        self.factory.stop()
        return



# fixme: broker doesn't handle endpoints disappearing.


