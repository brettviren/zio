#!/usr/bin/env python3

import zmq
import pyre
import zio.message as zm
import json
import time
import random

def now():
    return int(1000000*time.time())


# fixme/warning: this example mashes two ZIO layers together.  The
# "real" ZIO has a port layer between application and socket.  When
# you see "port" here, it's really a low level socket and the port
# function is hard-wired in the server methods.

class SimpleServer:
    def __init__(self, port, origin=42):
        self.port = port
        self.origin = origin
        self.credit = 0
        self.direction = ""
        self.count = 0
        self.client_id = 0;
        self.poller = zmq.Poller()
        self.poller.register(port, zmq.POLLIN)

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
            print ("send_pay(broke)")
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
        print("recv_pay(to=%d)", timeout)
        which  = dict(self.poller.poll(timeout))
        if not self.port in which:
            return False
        frame = self.port.recv(copy=False, timeout=timeout);
        if frame.routing_id != self.client_id:
            print ("unknown client")
            return
        parts = zm.decode_message(frame.bytes);
        ph = zm.PrefixHeader(parts[0])
        print("recv_pay",ph)
        fobj = json.loads(ph.label)
        if fobj["flow"] == "PAY":
            self.credit += fobj["credit"]
            return True
        return False

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

    def recv_dat(self, timeout=-1):
        self.send_pay();
        print("recv_dat()")
        which  = dict(self.poller.poll(timeout))
        if not self.port in which:
            return False
        frame = self.port.recv(copy=False);
        if frame.routing_id != self.client_id:
            print ("unknown client")
            return
        parts = zm.decode_message(frame.bytes);
        ph = zm.PrefixHeader(parts[0])
        print("recv_dat",ph)
        ch = zm.CoordHeader(parts[1])
        fobj = json.loads(ph.label)
        if fobj["flow"] == "DAT":
            self.credit += 1
            return True
        return False

    def eot(self, timeout=0):
        ph = zm.PrefixHeader(form="FLOW",
                             label=json.dumps(dict(flow="EOT")))
        ch = zm.CoordHeader(self.origin, now(), self.count)
        eot = zm.encode_message([bytes(ph), bytes(ch)])
        print ("send_eot", ph)
        self.port.send(eot, routing_id = self.client_id);
        while True:
            which  = dict(self.poller.poll(timeout))
            if not self.port in which:
                return False
            frame = self.port.recv(copy=False)
            parts = zm.decode_message(frame.bytes);
            ph = zm.PrefixHeader(parts[0])
            print("wait for eot, got",ph)
            fobj = json.loads(ph.label)
            if fobj["flow"] == "EOT":
                print ("got eot")
                return
            if timeout == 0:
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
I_quit = False
while True:
    print ("loop")
    if ss.is_sender:
        ss.send_dat()
    else:
        ok = ss.recv_dat()
        if not ok:
            break
    if random.uniform(0,1) > 0.8:
        I_quit = True
        break
if I_quit:
    print("sflow send EOT")
    ss.eot(-1)
else:
    print("sflow recv EOT")
    ss.eot(0)
print ("done")
node.stop()                     # else the program continues running
