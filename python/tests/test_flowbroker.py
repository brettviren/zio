#!/usr/bin/env python3
import json
import zmq
import zio

from zio.broker import FlowBroker, dumper
from pyre.zactor import ZActor

server_address = "tcp://127.0.0.1:5678"
def flow_maker():
    port = zio.Port("client", zmq.CLIENT,'')
    port.connect(server_address)
    port.online(None)       # peer not needed if port only direct connects
    print ("made flow")
    return zio.flow.Flow(port)


def client_actor(ctx, pipe, *args):
    pipe.signal()
    
    cflow = flow_maker()
    msg = zio.Message(label=json.dumps(dict(direction='extract',credit=2)))
    cflow.send_bot(msg)
    print ("client sent BOT", msg)
    msg = cflow.recv_bot(1000)
    assert(msg)
    print ("client got BOT:",msg)
    cflow.put(zio.Message())
    print ("client sent DAT")
    cflow.send_eot()
    print ("client sent EOT")
    msg = cflow.recv_eot()
    assert(msg)
    print ("client done")
    pipe.recv()                 # wait for signal to exit

def test_dumper():
    
    ctx = zmq.Context()

    node = zio.Node("dumper")
    server = node.port("server", zmq.SERVER)
    server.bind(server_address)
    node.online()

    client = ZActor(ctx, client_actor)
    actor = ZActor(ctx, dumper, flow_maker)
    broker = FlowBroker(server, actor.pipe)

    for count in range(10):
        print (f"main loop poll [{count}]")
        broker.poll(1000)      # client->handler



    broker.stop()
    node.offline()
    client.pipe.signal()
    

if '__main__' == __name__:
    test_dumper()
