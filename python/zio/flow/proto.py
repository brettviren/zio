#!/usr/bin/env python3

from zio.flow.sm import Flow as FlowMachine
import zio
import zmq

import logging
log = logging.getLogger(__name__)

class Flow(object):
    '''
    A Flow object provides ZIO flow protocol API

    It is equivalent to the C++ zio::Flow class

    All timeouts are in milliseconds.  A timeout of None means forever.  
    '''

    def __init__(self, port, direction, credit, timeout=None):
        '''
        Construct a flow object on a port.

        Application shall handle ports bind/connect and online states.
        '''
        self.port = port
        self.sm = FlowMachine(direction, credit)
        self.timeout = timeout

    @property
    def credit(self):
        return self.sm.credit
    @property
    def total_credit(self):
        return self.sm.total_credit
    

    def giver(self):
        return self.sm.direction == "extract"
    def taker(self):
        return self.sm.direction == "inject"

    def send_bot(self, msg=None):
        if msg is None:
            msg = zio.Message(form="FLOW")
        my_fobj = dict(flow='BOT',
                       credit = self.sm.total_credit,
                       direction = self.sm.direction)
        fobj = msg.label_object
        fobj.update(my_fobj)
        log.debug(f'send bot {fobj}')
        msg.label_object = fobj
        self.send(msg);
        

    def bot(self, msg=None):
        '''Do flow BOT handshake.  

        If msg given it is sent, else a generic one is created.

        Return the BOT message from other end
        '''
        my_fobj = dict(flow='BOT',
                       credit = self.sm.total_credit,
                       direction = self.sm.direction)

        if self.port.sock.type in (zmq.CLIENT, zmq.DEALER):
            self.send_bot(msg)
            return self.recv()

        if self.port.sock.type in (zmq.SERVER, zmq.ROUTER):
            obot = self.recv()

            # note: a server that wants to respond to a BOT, such as
            # the flow broker will want to do something more right here.

            if msg is None:
                msg = obot
            self.send_bot(msg)


    def eot(self, msg=None):
        '''Do flow EOT handshake.  

        If msg it given it is sent, else a generic one is created.

        Return the EOT message from the other end.
        '''
        self.eotsend(msg)
        return self.eotrecv()

    def eotsend(self, msg=None):
        '''Acknowledge the recipt of an EOT.

        If msg it given it is sent, else a generic one is created.

        This should be called if EOT is received during put() or get().
        '''
        if msg is None:
            msg = zio.Message(form='FLOW')
        fobj = msg.label_object
        fobj['flow'] = 'EOT'
        msg.label_object = fobj
        self.send(msg)
    def eotrecv(self):
        '''Recv EOT'''
        while True:
            msg = self.recv()
            if self.sm.state == 'ACKFIN':
                continue
            if self.sm.state != 'FIN':
                raise RuntimeError('flow remote error: EOT handshake failed to reach FIN state')
            break

    def put(self, dat):
        '''Send DAT message (for givers/extract) after checking for PAY'''
        self.recv_pay();
        if self.credit == 0:
            msg = self.port.recv(self.timeout)
            self.sm.RecvMsg(msg)
        fobj = dat.label_object
        fobj['flow'] = 'DAT'
        dat.label_object = fobj
        self.send(dat)
        return

    def get(self):
        '''Recv DAT message (for takers/inject) after flushing PAY
        
        DAT message is returned.'''
        self.send_pay()
        return self.recv()

    def pay(self):
        '''Manual pay, not required if put() or get() is used'''
        if self.giver():
            self.recv_pay()
        else:
            self.send_pay()
        return self.sm.credit

    def recv(self):
        '''Low-level recv of a message.

        Message is returned'''
        msg = self.port.recv(self.timeout)
        if self.sm.RecvMsg(msg):
            return msg
        return None

    def send(self, msg):
        '''Low-level send of a message.'''
        msg.form = 'FLOW'
        if not self.sm.SendMsg(msg):
            return 
        log.debug(f'flow send {msg} {msg.level} {msg.routing_id}')
        self.port.send(msg)
        
