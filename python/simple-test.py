#!/usr/bin/env python3

import zmq
import zio.message as zm
import json

ctx = zmq.Context()
sock = ctx.socket(zmq.SERVER)
sock.bind("tcp://127.0.0.1:5678")

while True:
    msg = sock.recv();
    print (type(msg))
    print (msg)
    parts = zm.decode(msg);
    ph = zm.parse_header_prefix(parts[0])
    print(ph)
    ch = zm.parse_header_coords(parts[1])
    print(ch)

    fobj = json.loads(ph[3])
    if fobj["direction"] == "extract":
        fobj["direction"] = "inject"
    else:
        fobj["direction"] = "extract"

    
