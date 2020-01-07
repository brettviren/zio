#!/usr/bin/env python3
'''
zio.flow implements ZIO flow protocol helper

This is equivalent to the C++ zio::flow namespace
'''
import json
import logging

from .message import Message

log = logging.getLogger(__name__)

from enum import Enum
class Direction(Enum):
    undefined=0
    extract=1
    inject=2

def objectify(morl):
    '''
    Return a flow object.

    The morl may be a zio.Message or a zio.Message.label
    '''
    if not morl:
        return dict()
    if type(morl) is bytes:
        morl = morl.decode('utf-8')
    if type(morl) is str:
        return json.loads(morl)
    return objectify(morl.prefix.label)

def stringify(flowtype, **params):
    '''
    Return a flow label string of given flow type and any extra
    parameters.
    '''
    params = params or dict()
    params['flow'] = flowtype
    return json.dumps(params)


class Flow:
    '''
    A Flow object provides ZIO flow protocol API

    It is equivalent to the C++ zio::flow::Flow class

    All timeouts are in milliseconds.
    '''

    credit = 0
    total_credit = 0
    is_sender = True
    routing_id = 0

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
        msg.form = 'FLOW'
        fobj = objectify(msg)
        msg.label = stringify('BOT', **fobj)
        msg.routing_id = self.routing_id
        self.port.send(msg)


    def recv_bot(self, timeout=-1):
        '''
        Receive and return BOT message or None.

        Returns None if EOT was received or timeout occurred.
        '''
        msg = self.port.recv(timeout)
        if msg is None:
            return None
        if msg.form != 'FLOW':
            return None
        fobj = objectify(msg)
        if fobj.get("flow",None) != "BOT":
            log.debug("malformed BOT flow: %s" % (msg,))
            return None

        credit = fobj.get("credit",None)
        if credit is None:
            log.debug("malformed BOT credit: %s" % (msg,))
            return None

        self.total_credit = credit

        fdir = fobj.get("direction", None)
        if fdir == "extract":
            self.is_sender = False
            self.credit = credit
        elif fdir == "inject":
            self.is_sender = True
            self.credit = 0
        else:
            log.debug("malformed BOT direction: %s" % (msg,))
            return None

        self.routing_id = msg.routing_id
        return msg
    
    def slurp_pay(self, timeout=-1):
        '''
        Receive any waiting PAY messages

        The flow object will slurp prior to a sending a DAT but the
        application may call this at any time after BOT.  Number of
        credits slurped is returned.  None is returned if other than a
        PAY is received.  Caller should likely respond to that with
        send_eot(msg,0).
        '''
        msg = self.port.recv(timeout)
        if msg is None:
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
        log.debug("send: %s" % msg)
        self.port.send(msg)
        return nsent


    def get(self, timeout=-1):
        '''
        Receive and return a DAT message and send any accumulated PAY.

        Return None if EOT was received instead.
        '''
        msg = self.port.recv(timeout)
        self.flush_pay()
        if msg is None:
            return None
        if msg.form != 'FLOW':
            return None
        fobj = objectify(msg)
        if fobj.get('flow',None) != 'DAT':
            log.debug("malformed DAT flow: %s" % (msg,))
            return None
        self.credit += 1
        self.flush_pay()
        return msg;
            


    def eot(self, msg, timeout=-1):
        '''
        Send EOT to other end and maybe wait for reply.

        Return EOT if recieved else None.

        Note: if calling in response to a received EOT, timeout is
        best set to 0.
        '''
        msg.form = 'FLOW'
        msg.label = stringify('EOT', **objectify(msg))
        msg.routing_id = self.routing_id
        self.port.send(msg)
        while True:
            msg = self.port.recv(timeout)
            if msg is None:
                return None
            if msg.form != 'FLOW':
                continue        # who's knocking at my door?
            fobj = objectify(msg)
            if fobj.get('flow',None) == 'EOT':
                return msg
            continue            # try again, probably got a PAY/DAT
        return                  # won't reach


    pass
