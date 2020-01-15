#!/usr/bin/env python3
'''
Test client, broker, spawner, factory, handler, writer, file chain.
'''
from zio.flow import Broker
from zio.flow.backend import spawner
from pyre.zactor import ZActor
from .factory import Factory

def flow_depos(ctx, pipe, nsend, name, address):
    '''
    An actor with a flow client sending depo messages.
    '''
    pipe.signal()

    port = zio.Port(name, zmq.CLIENT, '')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    flow = zio.flow.Flow(port)

    fobj = dict(flow='BOT', direction='extract', credit=2, stream=name)
    msg = zio.Message(form='FLOW',label=json.dumps(fobj))
    flow.send_bot(msg)
    msg = flow.recv_bot(1000)
    assert(msg)

    for count in range(nsend):
        depo = pb.Depo(ident=count,
                       pos=pb.Point(x=1,y=2,z=3),
                       time=100.0, charge=1000.0,
                       trackid=12345, pdg=11,
                       extent_long=9.6, extent_tran=6.9) 
        a = Any()
        a.Pack(depo)
        msg = zio.Message(form='FLOW',
                          label=json.dumps({'stream':stream}),
                          payload=[a.SerializeToString()])
        cflow.put(msg)
    port.offline()
    return

def make_broker(ctx, ruleset, address):
    sport = zio.Port("broker", zmq.SERVER);
    sport.bind(address)
    sport.do_binds()
    sport.online()    

    factory = Factory(ruleset, address)
    backend = ZActor(ctx, spawner, factory)
    broker = Broker(sport, backend.pipe)
    # fixme: need to manage backend.  For now, punt.
    broker.backend = backend
    return broker

def test_cbsfhwf():
    ctx = zmq.Context()

    server_address = "tcp://127.0.0.1:5678"

    client1 = ZActor(ctx, flow_depos, 10, "client1", server_address)

    broker = make_broker(ctx, ruleset, server_address)
    while True:
        ok = broker.poll(1000)
        if not ok:
            break;

    broker.stop()
    client1.pipe.signal()

if '__main__' == __name__:
    test_cbsfhwf()

