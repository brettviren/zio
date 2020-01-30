#!/usr/bin/env python3
'''
zio.flow implements ZIO flow protocol helper

This is equivalent to the C++ zio::flow namespace
'''


from ..message import Message
from .util import *

import logging
log = logging.getLogger("zpb")

from enum import Enum
class Direction(Enum):
    undefined=0
    extract=1
    inject=2

class Flow:
    '''
    A Flow object provides ZIO flow protocol API

    It is equivalent to the C++ zio::flow::Flow class

    All timeouts are in milliseconds.  A timeout of None means forever.
    '''

    credit = 0
    total_credit = 0
    is_sender = True
    routing_id = 0
    send_seqno = -1
    recv_seqno = -1

    def __init__(self, port):
        '''
        Construct a flow object on a port.

        Application shall handle ports bind/connect and online states.
        '''
        self.port = port

    def send_bot(self, msg):
        '''
        Send a BOT message to the other end.

        Client calls send_bot() first, server calls send_bot() second.
        '''
        if self.send_seqno != -1:
            raise RuntimeError("BOT must be sent first")

        msg.form = 'FLOW'
        fobj = objectify(msg)
        msg.label = stringify('BOT', **fobj)
        msg.routing_id = self.routing_id
        msg.seqno = self.send_seqno = 0
        log.debug(f'send_bot: {self.send_seqno} {msg}')
        self.port.send(msg)


    def recv_bot(self, timeout=-1):
        '''
        Receive and return BOT message or None.

        Returns None if EOT was received.  

        Raises exceptions
        '''
        if self.recv_seqno != -1:
            raise RuntimeError("BOT must be recv first")

        msg = self.port.recv(timeout)
        if msg is None:
            raise TimeoutError('flow.recv_bot: timeout')
        if msg.seqno != 0:
            raise RuntimeError(f'flow.recv_bot: BOT not seqno 0 {msg.seqno}')
        if msg.form != 'FLOW':
            raise TypeError('flow.recv_bot: not FLOW message')
        fobj = objectify(msg)
        ftype = fobj.get("flow",None) 
        if ftype != "BOT":
            raise TypeError(f'flow.recv_bot: unknown FLOW message {ftype}')

        credit = fobj.get("credit",None)
        if credit is None:
            raise ValueError('flow.recv_bot: no credit')

        fdir = fobj.get("direction", None)
        if fdir == "extract":
            self.is_sender = False
            self.credit = credit
        elif fdir == "inject":
            self.is_sender = True
            self.credit = 0
        else:
            raise ValueError(f'flow.recv_bot: uknonwn direction {fdir}')

        self.total_credit = credit
        self.recv_seqno = 0
        self.routing_id = msg.routing_id
        return msg
    
    def slurp_pay(self, timeout=None):
        '''
        Receive any waiting PAY messages

        The flow object will slurp prior to a sending a DAT but the
        application may call this at any time after BOT.  Number of
        credits slurped is returned.  None is returned if other than a
        PAY is received.  Caller should likely respond to that with
        send_eot(msg,0).
        '''
        if self.recv_seqno < 0:
            raise RuntimeError("must recv BOT before PAY")

        msg = self.port.recv(timeout)
        if msg is None:
            return None
        if msg.seqno - self.recv_seqno != 1:
            return None
        if msg.form != 'FLOW':
            return None
        fobj = objectify(msg)
        if fobj.get("flow",None) != "PAY":
            log.debug("malformed PAY flow: %s" % (msg,))
            return None
        try:
            credit = fobj["credit"]
        except KeyError:
            log.debug("malformed PAY credit: %s" % (msg,))
            return None
        self.credit += credit
        self.recv_seqno += 1
        return credit


    def put(self, msg):
        '''
        Send a DAT message and slurp for any waiting PAY.

        Return True if sent, None if an EOT was received instead of
        PAY.
        '''
        if self.credit < self.total_credit:
            self.slurp_pay(0)   # first do fast try
        while self.credit == 0: # next really must have 
            self.slurp_pay(-1)  # some credit to continue

        msg.form = 'FLOW'
        msg.label = stringify('DAT', **objectify(msg))
        msg.routing_id = self.routing_id
        if self.send_seqno < 0:
            raise RuntimeError("must send BOT before DAT")
        self.send_seqno += 1
        msg.seqno = self.send_seqno
        #log.debug (f"port send with credit %d: %s" % (self.credit, msg))
        self.port.send(msg)
        self.credit -= 1
        return True


    def flush_pay(self):
        '''
        Send any accumulated credit as PAY.

        This is called in a get() but app may call any time after a
        BOT exchange.  Number of credits sent is returned.  This does
        not block.
        '''
        if self.credit == 0:
            return 0
        nsent = self.credit
        self.credit = 0
        msg = Message(form='FLOW', label=stringify('PAY', credit=nsent))
        self.send_seqno += 1
        msg.seqno = self.send_seqno
        log.debug(f'flush_pay: {self.send_seqno} {msg}')
        if self.send_seqno < 0:
            raise RuntimeError("must recv BOT before PAY")
        self.port.send(msg)
        return nsent


    def get(self, timeout=None):
        '''
        Receive and return a DAT message and send any accumulated PAY.

        Return None if EOT was received instead of DAT.

        Exceptions raised.
        '''
        if self.recv_seqno < 0:
            raise RuntimeError("must recv BOT before DAT")

        log.debug (f'flow.get({timeout})')
        self.flush_pay()
        msg = self.port.recv(timeout)
        if msg is None:
            log.debug("flow.get timeout")
            raise TimeoutError("flow.get timeout")
        if msg.seqno - self.recv_seqno != 1:
            log.debug(f'flow.get bad seqno: {msg.seqno}, last {self.recv_seqno}\n{msg}')
            raise ValueError('flow.get bad seqno')
        if msg.form != 'FLOW':
            log.debug(f'flow.get not FLOW message\n{msg}')
            raise TypeError('flow.get not FLOW message')
        fobj = objectify(msg)
        if fobj.get('flow',None) == 'EOT':
            log.debug("EOT during flow:\n%s" % (msg,))
            return None
        ftype = fobj.get('flow',None)
        if ftype != 'DAT':
            log.warning("malformed DAT flow:\n%s" % (msg,))
            raise TypeError(f'flow.get unexpected FLOW type {ftype}')

        self.credit += 1
        self.recv_seqno += 1
        self.flush_pay()
        return msg;
            
    def send_eot(self, msg = Message()):
        '''
        Send EOT message to other end.

        Note, if app initiates the EOT, it should then call
        recv_eot().  If it unexpectedly got EOT when recving another
        then it should send EOT as a response.
        '''
        msg.form = 'FLOW'
        msg.label = stringify('EOT', **objectify(msg))
        msg.routing_id = self.routing_id
        if self.send_seqno < 0:
            raise RuntimeError("must send BOT before EOT")

        self.send_seqno += 1
        msg.seqno = self.send_seqno
        log.debug(f'send_eot: {self.send_seqno} {msg}')
        self.port.send(msg)
        self.send_seqno = -1

    def recv_eot(self, timeout=None):
        '''
        Recv an EOT message.

        EOT message is returned or None if timeout occurs.

        If app explicitly calls send_eot() it should call recv_eot()
        to wait for the ack from the other end.  If an app receives
        EOT as an unepxected message while receiving PAY or DAT then
        it should send_eot() but not expect another EOT ack.
        '''
        while True:
            if self.recv_seqno < 0:
                raise RuntimeError("must recv BOT before EOT")

            msg = self.port.recv(timeout)
            if msg is None:
                return None
            if msg.seqno - self.recv_seqno != 1:
                continue
            if msg.form != 'FLOW':
                continue        # who's knocking at my door?
            self.recv_seqno += 1
            fobj = objectify(msg)
            if fobj.get('flow',None) == 'EOT':
                self.recv_seqno = -1
                return msg
            continue            # try again, probably got a PAY/DAT
        return                  # won't reach



    pass
