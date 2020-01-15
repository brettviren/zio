#!/usr/bin/env python3
'''
Test client, broker, spawner, factory, handler, writer, file chain.
'''
import zmq
import json
from zio import Port, Message
from zio.flow import Flow, Broker
from zio.flow.backend import spawner
from pyre.zactor import ZActor
from factory import Factory
import wctzpb
from google.protobuf.any_pb2 import Any 

def flow_depos(ctx, pipe, nsend, name, address):
    '''
    An actor with a flow client sending depo messages.
    '''
    print (f'actor: flow_depos({nsend}, "{name}", "{address}"')

    pipe.signal()

    port = Port(name, zmq.CLIENT, '')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    flow = Flow(port)

    fobj = dict(flow='BOT', direction='extract', credit=2, stream=name)
    msg = Message(form='FLOW',label=json.dumps(fobj))
    flow.send_bot(msg)
    msg = flow.recv_bot(1000)
    assert(msg)

    for count in range(nsend):
        depo = wctzpb.pb.Depo(ident=count,
                              pos=wctzpb.pb.Point(x=1,y=2,z=3),
                              time=100.0,
                              charge=1000.0,
                              trackid=12345, pdg=11,
                              extent_long=9.6,
                              extent_tran=6.9) 
        a = Any()
        a.Pack(depo)
        msg = Message(form='FLOW',
                      label=json.dumps({'stream':name}),
                      payload=[a.SerializeToString()])
        flow.put(msg)

    flow.send_eot()
    flow.recv_eot()

    port.offline()
    return

def make_broker(ctx, ruleset, address):

    sock = ctx.socket(zmq.SERVER)
    sport = Port("broker", sock)
    sport.bind(address)
    sport.do_binds()
    sport.online()    

    print("make factory") 
    factory = Factory(ctx, ruleset, address,
                      wargs=(wctzpb.pb, wctzpb.tohdf))
    print("make backend") 
    backend = ZActor(ctx, spawner, factory)
    print("make broker") 
    broker = Broker(sport, backend.pipe)
    # fixme: need to manage the backend.  For now, punt.
    broker._backend_actor = backend
    return broker

def test_cbsfhwf():
    ctx = zmq.Context()

    server_address = "tcp://127.0.0.1:5678"
    ruleset = [
        dict(attr=dict(testname="cbsfhwf"), # just because
             rw="w",
             rule='(= stream "client1")',
             filepat="test-{testname}.hdf",
             grouppat="{stream}"),
    ]

    client1 = ZActor(ctx, flow_depos, 10, "client1", server_address)
    broker = make_broker(ctx, ruleset, server_address)

    print ("start broker poll")
    while True:
        ok = broker.poll(1000)
        if not ok:
            break;

    broker.stop()
    client1.pipe.signal()

if '__main__' == __name__:
    test_cbsfhwf()

