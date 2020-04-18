#!/usr/bin/env python3

from transitions.extensions import HierarchicalMachine as Machine

import logging
# logging.basicConfig(level=logging.DEBUG)
# Set transitions' log level to INFO; DEBUG messages will be omitted
logging.getLogger('transitions').setLevel(logging.INFO)

def tran(t,s,d,c,a):
    return dict(trigger=t, source=s, dest=d, conditions=c,after=a)


class Flow(object):

    states = ['IDLE', 'BOTSEND', 'BOTRECV', 'READY',
              dict(name='giving',
                   initial='BROKE',
                   children=['BROKE', 'GENEROUS']),
              dict(name='taking',
                   initial='RICH',
                   children=['RICH', 'HANDSOUT']),
              'FINACK','ACKFIN','FIN'
    ]
    transitions =[
        # bot handshake
        tran('SendMsg','IDLE','BOTSEND','check_send_bot','send_msg'),
        tran('RecvMsg','IDLE','BOTRECV','check_recv_bot','recv_bot'),
        tran('SendMsg','BOTRECV','READY','check_send_bot','send_msg'),
        tran('RecvMsg','BOTSEND','READY','check_recv_bot','recv_bot'),
        tran('BeginFlow','READY','giving','is_giver',None),
        tran('BeginFlow','READY','taking','is_taker',None),

        # giving submachine
        tran('RecvMsg','BROKE','GENEROUS','check_pay','recv_pay'),
        tran('SendMsg','GENEROUS','BROKE','check_one_credit','send_msg'),
        tran('SendMsg','GENEROUS','GENEROUS','check_many_credit','send_msg'),
        tran('RecvMsg','GENEROUS','GENEROUS','check_pay','recv_pay'),

        # taking submachine
        tran('FlushPay','RICH','HANDSOUT','have_credit','flush_pay'),
        tran('RecvMsg','HANDSOUT','RICH','check_last_credit','recv_dat'),
        tran('RecvMsg','HANDSOUT','HANDSOUT','check_many_credit','recv_dat'),
        tran('FlushPay','HANDSOUT','HANDSOUT','check_have_credit','flush_pay'),
        
        # eot response
        tran('SendMsg','giving','ACKFIN','check_eot','send_msg'),
        tran('RecvMsg','giving','FINACK','check_eot','recv_eot'),
        tran('SendMsg','taking','ACKFIN','check_eot','send_msg'),
        tran('RecvMsg','taking','FINACK','check_eot','recv_eot'),

        tran('SendMsg','FINACK','FINACK','not_check_eot',None),
        tran('RecvMsg','FINACK','FINACK','not_check_eot',None),
        tran('SendMsg','FINACK','FIN','check_eot','send_msg'),

        
        tran('RecvMsg','ACKFIN','ACKFIN','not_check_eot',None),
        tran('SendMsg','ACKFIN','ACKFIN','not_check_eot',None),
        tran('RecvMsg','ACKFIN','FIN','check_eot','recv_eot'),

    ]

    send_seqno = -1
    recv_seqno = -1
    credit=0
    total_credit=0
    remid = 0

    def __init__(self, direction, credit):
        self.machine = Machine(model=self, states=Flow.states, initial='IDLE',
                               transitions=Flow.transitions)
        self.direction = direction
        self.total_credit = credit

    # guards

    def is_giver(self,msg):
        return self.direction == "extract"

    def is_taker(self,msg):
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
            return False
        fobj = msg.label_object
        if fobj.get('flow',None) != 'BOT':
            return False
        odir = fobj.get('direction',None)
        if odir != self.direction:
            return False
        return True

    def check_pay(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'PAY':
            return False
        got_cred = fobj.get('credit', -1)
        if got_cred < 0:
            return False
        if got_cred + self.credit > self.total_credit:
            return False
        return True

    def check_eot(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'EOT':
            return False
        return True
    def not_check_eot(self,msg):
        return not self.check_eot(msg)

    def check_dat(self,msg):
        fobj = msg.label_object
        if fobj.get('flow',None) != 'DAT':
            return False
        return True


    def check_one_credit(self,msg):
        return self.credit == 1

    def check_last_credit(self,msg):
        return self.total_credit - self.credit == 1

    def check_many_credit(self,msg):
        return self.credit > 1

    def check_have_credit(self,msg):
        return self.credit > 0


    def recv_bot(self, msg):
        fobj = msg.label_object
        offer_credit = fobj["credit"]
        if self.server:
            if offer_credit > 0 and offer_credit < self.total_credit:
                self.total_credit = offer_credit
        else:                   # client
            self.total_credit = offer_credit
        self.credit = 0
        if self.direction == "extract":
            self.credit = self.total_credit
        self.recv_seqno += 1
        self.remid = msg.routing_id

    def send_msg(self, msg):
        self.send_seqno += 1
        msg.seqno = self.send_seqno
        msg.routing_id = self.remid

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
        fobj = msg.label_object
        fobj["flow"] = "PAY"
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

