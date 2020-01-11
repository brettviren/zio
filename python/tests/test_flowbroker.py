#!/usr/bin/env python3
import json
import zmq
import zio
from zio.flow import objectify, Broker

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


def dumper(ctx, pipe, flow_factory, *args):
    '''
    A dump handler which may be used as an actor talking to a broker's botport.

    Parameters
    ----------
    flow_factory : a callable
        Each call shall return an online zio.Flow.
    '''
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    s2f = dict()                # socket to its flow

    while True:

        for sock,_ in poller.poll():

            if sock == pipe:
                print ("dumper: pipe hit")
                data = pipe.recv()

                if data == b'STOP':
                    print ("dumper: got STOP")
                    return
                if len(data) == 0:
                    print ("dumper: got signal")
                    return

                bot = zio.Message(encoded=data)
                fobj = objectify(bot)
                if fobj['direction'] != 'inject':
                    print("dumper: rejecting bot",bot)
                    pipe.send_string('NO')
                    continue
                pipe.send_string('OK')
                flow = flow_factory()
                poller.register(flow.port.sock, zmq.POLLIN)
                s2f[flow.port.sock] = flow
                print ("dumper: BOT from pipe:",bot)
                flow.send_bot(bot)
                bot = flow.recv_bot()
                print ("dumper: BOT from sock:",bot)
                flow.flush_pay() # we are inject
                print("dumper: handled pipe")
                continue

            flow = s2f[sock]
            msg = flow.get()
            print ("dumper: sock hit:",msg)
            if msg is None:
                print ("dumper: null message from get, sending EOT")
                flow.send_eot()
                del s2f[sock]
                poller.unregister(sock)
                continue
            print ("dumper: msg: ",msg)
    return

def test_dumper():
    
    ctx = zmq.Context()

    node = zio.Node("dumper")
    server = node.port("server", zmq.SERVER)
    server.bind(server_address)
    node.online()

    client = ZActor(ctx, client_actor)
    actor = ZActor(ctx, dumper, flow_maker)
    broker = Broker(server, actor.pipe)

    for count in range(10):
        print (f"main: poll [{count}]")
        broker.poll(1000)      # client->handler

    print (f"main: stop broker")
    broker.stop()
    print (f"main: node offline")
    node.offline()
    print (f"main: stop client")
    client.pipe.signal()
    

if '__main__' == __name__:
    test_dumper()
