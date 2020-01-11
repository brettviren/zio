#!/usr/bin/env python3
'''
Test flow broker with example handler
'''
import zmq
import zio
import json
from zio.flow import objectify, Broker
from zio.flow.example_handler import Factory
from zio.flow.backend import spawner
from pyre.zactor import ZActor

server_address = "tcp://127.0.0.1:5679"

def client_actor(ctx, pipe, *args):
    pipe.signal()
    
    port = zio.Port("client", zmq.CLIENT,'')
    port.connect(server_address)
    port.online(None)       # peer not needed if port only direct connects
    cflow = zio.flow.Flow(port)

    msg = zio.Message(label=json.dumps(dict(direction='extract',credit=2)))
    cflow.send_bot(msg)
    print ("client sent BOT:\n%s\n" % (msg,))
    msg = cflow.recv_bot(1000)
    assert(msg)
    print ("client got BOT:\n%s\n" % (msg,))

    for count in range(10):
        cflow.put(zio.Message())

    print ("client sent DAT")
    cflow.send_eot()
    print ("client sent EOT")
    msg = cflow.recv_eot()
    assert(msg)
    print ("client done")
    pipe.recv()                 # wait for signal to exit


def test_broker():

    ctx = zmq.Context()

    # note: normally, app goes through zio.Node
    sport = zio.Port("server", zmq.SERVER);
    sport.bind(server_address)
    sport.do_binds()
    sport.online()

    client = ZActor(ctx, client_actor)

    factory = Factory(server_address)
    backend = ZActor(ctx, spawner, factory)
    broker = Broker(sport, backend.pipe)

    for count in range(30):
        print (f"main: poll [{count}]")
        ok = broker.poll(1000)      # client->handler
        if not ok:
            break

    print ("main: stopping")
    broker.stop()
    client.pipe.signal()

if '__main__' == __name__:
    test_broker()
    
