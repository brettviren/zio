#!/usr/bin/env python3

import zmq
import pyre
import zio.message as zm
import json
import time

ctx = zmq.Context()
sock = ctx.socket(zmq.SERVER)
addr = "tcp://127.0.0.1:5678"
sock.bind(addr)

# FIXME: this needs to all be put behind the python equivalent to
# zio::Peer.
node = pyre.Pyre("testflows")
portname = "recver"
node.set_header("zio.port.%s.address"%portname, addr)
node.set_header("zio.port.%s.socket"%portname, "SERVER")
node.start()

while True:
    frame = sock.recv(copy=False);
    print (type(frame))
    print (frame.bytes)
    parts = zm.decode_message(frame.bytes);
    ph = zm.PrefixHeader(parts[0])
    print(ph)
    ch = zm.CoordHeader(parts[1])
    print(ch)

    fobj = json.loads(ph.label)
    if fobj["direction"] == "extract":
        fobj["direction"] = "inject"
    else:
        fobj["direction"] = "extract"
    ph.label = json.dumps(fobj)

    ch.origin = 42
    ch.granule = int(time.time())

    ret = zm.encode_message([bytes(ph), bytes(ch)])
    print (ret)
    sock.send(ret, routing_id = frame.routing_id)
        
