#!/usr/bin/env python3

from zio.flow.sm import Flow as FlowMachine
import zio
import zmq

import logging
log = logging.getLogger(__name__)

class TransmissionEnd(Exception):
    def __init__(self, text, msg):
        self.message = text
        self.msg = msg;


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
        serverish = port.sock.type in (zmq.SERVER,zmq.ROUTER)
        self.sm = FlowMachine(direction, credit, serverish)
        self.timeout = timeout

    @property
    def credit(self):
        return self.sm.credit

    @property
    def total_credit(self):
        return self.sm.total_credit
    
    @property
    def name(self):
        return self.port.name

    @property
    def serverish(self):
        return self.sm.serverish

    def debug(self, text):
        sc = 'c'
        if self.serverish:
            sc='s'
        d = '->'
        if self.taker():
            d = '<-'
        prefix = f'flow [{sc}] {self.name} {d} {self.credit}/{self.total_credit} '
        log.debug(prefix + text)

    def giver(self):
        return self.sm.direction == "extract"
    def taker(self):
        return self.sm.direction == "inject"

    def send_bot(self, msg=None):
        if msg is None:
            msg = zio.Message()
        msg.form = "FLOW"
        my_fobj = dict(flow='BOT',
                       credit = self.sm.total_credit,
                       direction = self.sm.direction)
        fobj = msg.label_object
        fobj.update(my_fobj)
        self.debug(f'send_bot: {fobj}')
        msg.label_object = fobj
        self.send(msg);
        

    def bot(self, msg=None):
        '''Do flow BOT handshake.  

        If msg given it is sent, else a generic one is created.

        Return the BOT message from other end
        '''
        if self.serverish:
            # serverish
            obot = self.recv()

            # note: a server that wants to respond to a BOT, such as
            # the flow broker will want to do something more right here.

            if msg is None:
                msg = obot

            self.send_bot(msg)
            ret = obot
        else:
            self.send_bot(msg)
            ret = self.recv()
        return ret

    def begin(self):
        if not self.sm.BeginFlow():
            raise RuntimeError("flow failed to begin")
        self.pay()

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
            return msg

    def put(self, dat):
        '''Send DAT message (for givers/extract) after checking for PAY'''
        self.recv_pay();
        fobj = dat.label_object
        fobj['flow'] = 'DAT'
        dat.label_object = fobj
        self.send(dat)
        return

    def get(self):
        '''Recv DAT message (for takers/inject) after flushing PAY
        
        DAT message is returned.'''
        self.send_pay()
        ret = self.recv()
        assert (self.sm.credit > 0)
        fobj = ret.label_object
        if fobj['flow'] == 'EOT':
            raise TransmissionEnd("EOT in flow get",ret)
        return ret

    def pay(self):
        '''Manual pay, not required if put() or get() is used'''
        if self.giver():
            self.recv_pay()
        else:
            self.send_pay()
        return self.sm.credit

    def recv_pay(self):
        if self.taker():        # sanity check application
            raise TypeError('flow extract does not receive pay')
        if self.credit < self.total_credit:
            self.debug(f'recv_pay: flow has room for pay in {self.sm.state}')
            msg = self.port.recv(0)
            if msg is None:     # timeout
                return
            if not self.sm.RecvMsg(msg):
                raise RuntimeError(f'flow recv bad PAY: {msg} in {self.sm.state}')
        if not self.credit:
            self.debug('recv_pay: flow needs pay badly')
            msg = self.port.recv(self.timeout)
            if msg is None:
                raise TimeoutError('flow timeout waiting for needed credit')
            if not self.sm.RecvMsg(msg):
                raise RuntimeError(f'flow recv bad PAY: {msg} in {self.sm.state}')

    def send_pay(self):
        if self.giver():        # sanity check application
            raise TypeError('flow inject does not snd pay')
        if not self.credit:
            self.debug("send_pay: no credit to send")
            return
        pay = zio.Message(form='FLOW', label_object={'flow':'PAY'})
        if not self.sm.FlushPay(pay):
            raise RuntimeError('flow send pay failed')
        if self.credit:
            raise RuntimeError('LOGIC ERROR')
        self.debug(f"send_pay: {pay}")
        self.port.send(pay)

    def recv(self):
        '''Low-level recv of a message.

        Message is returned'''
        msg = self.port.recv(self.timeout)
        self.debug(f'recv: from {self.sm.state} {msg} rid:{msg.routing_id}')
        if not self.sm.RecvMsg(msg):
            raise ValueError(f'bad recv {msg} in {self.sm.state}')
        self.debug(f'recv: remid:{self.sm.remid}')
        return msg

    def send(self, msg):
        '''Low-level send of a message.'''
        msg.form = 'FLOW'
        self.debug(f'send: from {self.sm.state} {msg} rid:{msg.routing_id}')
        if not self.sm.SendMsg(msg):
            raise ValueError(f'bad send {msg} in {self.sm.state}')
        self.debug(f'send: {msg} rid:{msg.routing_id}')
        self.port.send(msg)
        
