#!/usr/bin/env python3
'''
ZIO brokers bring together ZIO clients.

'''

import json
from collections import namedtuple
import zmq
import zio
from zio.flow import objectify

def switch_direction(fobj):
    fobj = dict(fobj)
    if fobj["direction"] == 'inject':
        fobj["direction"] = 'extract'
    elif fobj["direction"] == 'extract':
        fobj["direction"] = 'inject'
    else:
        return None
    return fobj

        

class FlowBroker:
    def __init__(self, server, backend):
        '''Create a flow broker.

        Parameters
        ----------
        server : zio.Port
            An online SERVER port
        backend : zmq.socket
            See below
        
        This broker routes messages between a pair of clients: a
        remote client and its handler.  It does this by adding a flow
        handler protocol to ZIO flow protocol.

        The flow handler protocol is spoken by the broker on its
        backend socket.  When a new BOT flow message is received from
        the server port it will be checked for the existence of a
        'cid' attribute in the flow object.  If not existing then the
        BOT came from a remote client.  The BOT direction is switched
        and sent to the backend socket and the broker waits for a
        response.
  
        The response shall be a simple string message holding either
        "OK" or "NO".  If "NO", an immediate EOT message is sent to
        the remote client.  Otherwise, the broker expects some future
        BOT message on the server port which includes this `cid`.
        Once that BOT is received by the server any subsequent PAY,
        DAT or EOT will be exchanged between client and handler.

        Note that the flow handler protocol does not communicate the
        location of the broker's SERVER socket.  It is up to handler
        implementations to locate the the server.

        '''
        self.server = server
        self.backend = backend
        self.other = dict()     # map router IDs

    def poll(self, timeout=None):
        '''
        Poll for at most one message from the SERVER port

        Return None if timeout, False if error, True if nominal
        '''
        msg = self.server.recv(timeout)
        if not msg:
            #print ("broker poll timeout:",timeout)
            return None
        #print ("broker poll:",msg)
        rid = msg.routing_id
        fobj = objectify(msg)
        ftype = fobj.get("flow", None)
        if not ftype:
            return False

        # Special, assymetric handling of BOT
        if ftype == "BOT":
            cid = fobj.get('cid',None)
            if cid:             # BOT from handler
                self.other[rid] = cid
                self.other[cid] = rid
                fobj = switch_direction(fobj)                
                del fobj["cid"]
                orig_label = msg.label
                msg.label = json.dumps(fobj)                
                msg.routing_id = rid
                self.server.send(msg) # mirror back to handler
                msg.label = orig_label
                # fall through to send to client
                
            else:               # BOT from client
                fobj = switch_direction(fobj)
                if fobj is None:
                    return False 
                fobj["cid"] = rid
                msg.label = json.dumps(fobj)
                msg.routing_id = 0
                self.backend.send(msg.encode())
                got = self.backend.recv_string()
                if got == 'OK':
                    return True
                if got == 'NO':
                    fobj['flow'] = 'EOT'
                    msg.label = json.dumps(fobj)
                    msg.routing_id = rid
                    self.server.send(msg)
                    return False
                return False    # unknown respose 

        # route message to other
        cid = self.other[rid]
        msg.routing_id = cid
        print (f"broker route {rid} -> {cid}: {msg}")
        self.server.send(msg)

        # if EOT comes through, we should forget the routing
        if ftype == 'EOT':
            del self.other[rid]

        return True

    def stop(self):
        '''
        Stop the broker by sending a signal backend.
        '''
        self.backend.send_string("STOP")

# fixme: broker doesn't handle endpoints disappearing.


