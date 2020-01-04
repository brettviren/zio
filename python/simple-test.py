#!/usr/bin/env python3

import zmq
import pyre
import zio.message as zm
import json
import time


def now():
    return int(1000000*time.time())


class SimpleServer:
    def __init__(self, port, origin=42):
        self.port = port
        self.origin = origin
        self.credit = 0
        self.direction = ""
        self.count = 0
        self.client_id = 0;

    def do_bot(self):
        frame = self.port.recv(copy=False);
        print (type(frame))
        print (frame.bytes)
        parts = zm.decode_message(frame.bytes);
        ph = zm.PrefixHeader(parts[0])
        print(ph)
        ch = zm.CoordHeader(parts[1])
        print(ch)

        fobj = json.loads(ph.label)

        credit = fobj["credit"]
        direction = fobj["direction"]

        if direction == "extract":
            self.direction = "inject"
            self.credit = credit
            self.is_sender = False
        elif direction == "inject":
            self.direction = "extract"
            self.credit = 0
            self.is_sender = True
        else:
            print ("unknown direction %s" % direction)
            return
        fobj["direction"] = self.direction

        ph.label = json.dumps(fobj)
        ch.origin = self.origin
        ch.granule = now()

        ret = zm.encode_message([bytes(ph), bytes(ch)])
        print (ret)
        self.client_id = frame.routing_id
        self.port.send(ret, routing_id = frame.routing_id)

    def send_pay(self):
        if self.credit == 0:
            return
        ph = zm.PrefixHeader(form="FLOW",
                             label=json.dumps(dict(flow="PAY",
                                                   credit=self.credit)))
        ch = zm.CoordHeader(self.origin, now(), self.count)
        self.count += 1
        self.credit = 0
        pay = zm.encode_message([bytes(ph), bytes(ch)])
        print ("send_pay",ph)
        self.port.send(pay, routing_id = self.client_id)

    def recv_pay(self, timeout=0):
        frame = self.port.recv(copy=False, timeout=timeout);
        if frame.routing_id != self.client_id:
            print ("unknown client")
            return
        parts = zm.decode_message(frame.bytes);
        ph = zm.PrefixHeader(parts[0])
        print("recv_pay",ph)
        fobj = json.loads(ph.label)
        self.credit += fobj["credit"]

    def send_dat(self):
        self.recv_pay(0)
        if self.credit == 0:
            self.recv_pay(-1)
        if self.credit == 0:
            raise RuntimeError("no credit and failed to get payed")
        ph = zm.PrefixHeader(form="FLOW", label=json.dumps({'flow':'DAT'}))
        ch = zm.CoordHeader(self.origin, now(), self.count)
        self.count += 1
        self.credit -= 1
        dat = zm.encode_message([bytes(ph), bytes(ch)])
        print ("send_dat",ph)
        self.port.send(day, routing_id = self.client_id)

    def recv_dat(self):
        self.send_pay();
        frame = self.port.recv(copy=False);
        if frame.routing_id != self.client_id:
            print ("unknown client")
            return
        self.count += 1
        parts = zm.decode_message(frame.bytes);
        ph = zm.PrefixHeader(parts[0])
        print("recv_dat",ph)
        ch = zm.CoordHeader(parts[1])
        return




#### this is app level code
ctx = zmq.Context()
sock = ctx.socket(zmq.SERVER)
addr = "tcp://127.0.0.1:5678"
sock.bind(addr)

node = pyre.Pyre("testflows")
portname = "recver"
node.set_header("zio.port.%s.address"%portname, addr)
node.set_header("zio.port.%s.socket"%portname, "SERVER")
node.start()
##############


ss = SimpleServer(sock);
ss.do_bot()
while True:
    if ss.is_sender:
        ss.send_dat()
    else:
        ss.recv_dat()
    
