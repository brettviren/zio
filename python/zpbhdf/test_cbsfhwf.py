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

import logging
logging.basicConfig(format='%(asctime)s %(levelname)10s %(name)-20s - %(message)s',
                    datefmt='%H:%M:%S',
                    level=logging.DEBUG)
log = logging.getLogger('test-cbsfhwf')
log.level = logging.DEBUG

def flow_depos(ctx, pipe, nsend, name, address):
    '''
    An actor with a flow client sending depo messages.
    '''
    log.debug(f'actor: flow_depos({nsend}, "{name}", "{address}"')

    pipe.signal()

    port = Port(name, zmq.CLIENT, '')
    port.connect(address)
    port.online(None)       # peer not needed if port only direct connects
    flow = Flow(port)

    fobj = dict(flow='BOT', direction='extract', credit=3, stream=name)
    msg = Message(seqno=0, form='FLOW',label=json.dumps(fobj))
    log.debug (f'flow_depos {name} send BOT:\n{msg}')
    flow.send_bot(msg)
    msg = flow.recv_bot(1000)
    log.debug (f'flow_depos {name} got BOT:\n{msg}')
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
        msg = Message(form='FLOW',seqno=count+1,
                      label=json.dumps({'flow':'DAT'}),
                      payload=[a.SerializeToString()])
        log.debug (f'flow_depos {name} put: {count}/{nsend}[{flow.credit}]:\n{msg}')
        flow.put(msg)
        log.debug (f'flow_depos {name} again [{flow.credit}]')

    log.debug (f'flow_depos {name} send EOT')
    flow.send_eot(Message(seqno=nsend+1))
    log.debug (f'flow_depos {name} recv EOT')
    flow.recv_eot()
    log.debug (f'flow_depos {name} wait for quit signal')
    pipe.recv()                 # wait for signal to quit
    log.debug(f'flow_depos {name} exiting')

    return

def make_broker(ctx, ruleset, address):

    sock = ctx.socket(zmq.SERVER)
    sport = Port("broker", sock)
    sport.bind(address)
    sport.do_binds()
    sport.online()    

    log.debug("make factory") 
    factory = Factory(ctx, ruleset, address,
                      wargs=(wctzpb.pb, wctzpb.tohdf))
    return Broker(sport, factory)

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

    log.debug ("start broker poll")
    while True:
        ok = broker.poll(1000)
        if ok is None:
            log.debug(f'broker is too lonely')
            break;

    log.debug("broker stop")
    broker.stop()
    log.debug("client1 pipe signal")
    client1.pipe.signal()
    log.debug ("test done")

if '__main__' == __name__:
    test_cbsfhwf()

