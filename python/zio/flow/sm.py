#!/usr/bin/env python3

from transitions.extensions import HierarchicalMachine as Machine

import logging
# logging.basicConfig(level=logging.DEBUG)
# Set transitions' log level to INFO; DEBUG messages will be omitted
# logging.getLogger('transitions').setLevel(logging.INFO)
log = logging.getLogger(__name__)

def tran(t,s,d,**kwds):
    ret = dict(trigger=t, source=s, dest=d)
    ret.update(kwds)
    return ret

states = [
    'IDLE', 'BOTSEND', 'BOTRECV', 'READY',
    'FINACK','ACKFIN','FIN',
    dict(name='giving',
         initial='BROKE',
         children=['BROKE', 'GENEROUS'],
         transitions=[
             tran('RecvMsg','BROKE','GENEROUS',
                  conditions='check_pay', after='recv_pay'),
             tran('RecvMsg','GENEROUS','GENEROUS',
                  conditions='check_pay', after='recv_pay'),
             tran('SendMsg','GENEROUS','BROKE',
                  conditions=['check_one_credit','is_dat'], after='send_dat'),
             tran('SendMsg','GENEROUS','GENEROUS',
                  conditions=['check_many_credit','is_dat'], after='send_dat'),
         ]
    ),
    dict(name='taking',
         initial='RICH',
         children=['RICH', 'HANDSOUT'],
         transitions=[
             tran('FlushPay','RICH','HANDSOUT',
                  conditions=['check_have_credit','is_pay'], after='flush_pay'),
             tran('FlushPay','HANDSOUT','HANDSOUT',
                  conditions=['check_have_credit','is_pay'], after='flush_pay'),
             tran('RecvMsg','HANDSOUT','RICH',
                  conditions=['check_last_credit','is_dat'], after='recv_dat'),
             tran('RecvMsg','HANDSOUT','HANDSOUT',
                  conditions=['check_low_credit','is_dat'], after='recv_dat'),
         ]
    )
]
transitions =[
    # bot handshake
    # bot handshake
    tran('SendMsg','IDLE','BOTSEND',
         conditions='check_send_bot',after='send_msg'),
    tran('RecvMsg','IDLE','BOTRECV',
         conditions='check_recv_bot',after='recv_bot'),
    tran('SendMsg','BOTRECV','READY',
         conditions='check_send_bot',after='send_msg'),
    tran('RecvMsg','BOTSEND','READY',
         conditions='check_recv_bot',after='recv_bot'),

    tran('BeginFlow','READY','giving',
         conditions='is_giver'),
    tran('BeginFlow','READY','taking',
         conditions='is_taker'),
    
        # eot response
    tran('SendMsg','giving','ACKFIN',
         conditions='check_eot', after='send_msg'),
    tran('RecvMsg','giving','FINACK',
         conditions='check_eot', after='recv_eot'),
    tran('SendMsg','taking','ACKFIN',
         conditions='check_eot', after='send_msg'),
    tran('RecvMsg','taking','FINACK',
         conditions='check_eot', after='recv_eot'),

    tran('SendMsg','FINACK','FINACK',
         unless='check_eot'),
    tran('RecvMsg','FINACK','FINACK',
         unless='check_eot'),
    tran('SendMsg','FINACK','FIN',
         conditions='check_eot',after='send_msg'),
        
    tran('RecvMsg','ACKFIN','ACKFIN',
         unless='check_eot'),
    tran('SendMsg','ACKFIN','ACKFIN',
         unless='check_eot'),
    tran('RecvMsg','ACKFIN','FIN',
         conditions='check_eot'),

]

class Flow(object):

    send_seqno = -1
    recv_seqno = -1
    credit=0
    total_credit=0
    remid = None

    def __init__(self, direction, credit, serverish):
        self.machine = Machine(model=self, states=states, initial='IDLE',
                               transitions=transitions)
        self.direction = direction
        self.total_credit = credit
        self.serverish = serverish
        #log.info(f'STATES: {self.machine.states}')

    # guards

    def is_giver(self, msg=None):
        return self.direction == "extract"

    def is_taker(self, msg=None):
        return self.direction == "inject"

    def check_send_bot(self,msg):
        if self.send_seqno != -1:
            return False
        fobj = msg.label_object
        if fobj.get('flow',None) != 'BOT':
            return False
        odir = fobj.get('direction',None)
        if odir != self.direction:
            return False
        return True

    def check_recv_bot(self,msg):
        if self.recv_seqno != -1:
            log.debug(f'bad recv_seqno for BOT: {self.recv_seqno}')
            return False
        fobj = msg.label_object
        if fobj.get('flow',None) != 'BOT':
            log.debug('bad flow type for BOT')
            return False
        odir = fobj.get('direction',None)
        if odir == self.direction:
            log.debug(f'bad flow direction for BOT: {odir}')
            return False
        return True

    def is_pay(self, msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'PAY':
            log.debug(f'not pay {msg}')
            return False
        return True

    def is_dat(self, msg):
        fobj = msg.label_object
        return fobj.get('flow',None) == 'DAT'

    def check_pay(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'PAY':
            log.debug(f'not pay type {msg}')
            return False
        got_cred = fobj.get('credit', -1)
        if got_cred < 0:
            log.debug(f'negative pay {msg}')
            return False
        if got_cred + self.credit > self.total_credit:
            log.debug(f'too much pay: {got_cred}+{self.credit}>{self.total_credit} {msg}')
            return False
        return True

    def check_eot(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) == 'EOT':
            return True
        return False

    def check_dat(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'DAT':
            return False
        return True


    def check_one_credit(self,msg=None):
        ok = self.credit == 1
        if not ok:
            log.debug(f'check_one_credit {self.credit}/{self.total_credit} {msg}')
        return ok

    def check_last_credit(self,msg=None):
        ok = self.total_credit - self.credit == 1
        if not ok:
            log.debug(f'check_last_credit {self.credit}/{self.total_credit} {msg}')
        return ok

    def check_low_credit(self,msg=None):
        ok = self.total_credit - self.credit > 1
        if not ok:
            log.debug(f'check_low_credit {self.credit}/{self.total_credit} {msg}')
        return ok


    def check_many_credit(self,msg=None):
        ok = self.credit > 1
        if not ok:
            log.debug(f'check_many_credit {self.credit}/{self.total_credit} {msg}')
        return ok

    def check_have_credit(self,msg=None):
        ok = self.credit > 0
        if not ok:
            log.debug(f'check_have_credit {self.credit}/{self.total_credit} {msg}')
        return ok



    def recv_bot(self, msg):
        fobj = msg.label_object
        offer_credit = fobj["credit"]
        if self.serverish:      # server may lower to client's offer
            if offer_credit > 0 and offer_credit < self.total_credit:
                self.total_credit = offer_credit
        else:                   # client must accept server's offer
            self.total_credit = offer_credit
        self.credit = 0
        if self.direction == "inject":
            self.credit = self.total_credit
        self.recv_seqno += 1
        self.remid = msg.routing_id

    def send_msg(self, msg):
        self.send_seqno += 1
        msg.seqno = self.send_seqno
        msg.routing_id = self.remid

    def send_dat(self, msg):
        self.credit -= 1
        self.send_msg(msg);

    def recv_eot(self, msg):
        self.recv_seqno += 1

    def recv_dat(self, msg):
        self.recv_seqno += 1
        self.credit += 1

    def recv_pay(self, msg):
        self.recv_seqno += 1
        fobj = msg.label_object
        self.credit += fobj["credit"]

    def flush_pay(self, msg):
        msg.form='FLOW'
        fobj = msg.label_object
        fobj["credit"] = self.credit
        msg.label_object = fobj
        self.send_seqno += 1
        msg.seqno = self.send_seqno
        self.credit = 0


def self_test():
    g = Flow("extract", 10)
    g.SendMsg("bot")
    g.RecvMsg("bot")
    g.BeginFlow("flow")
    g.RecvMsg("pay")
    g.RecvMsg("eot")
    g.SendMsg("eot")

