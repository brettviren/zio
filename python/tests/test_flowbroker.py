#!/usr/bin/env python3
import json
import zmq
import zio
from zio.dump import handler
from zio.broker import FlowBroker
from pyre.zactor import ZActor


def test_dumper():
    
    ctx = zmq.Context()

    server_address = "tcp://127.0.0.1:5678"
    def flow_maker():
        port = zio.Port("client", zmq.CLIENT,'')
        port.connect(server_address)
        port.online(None)       # peer not needed if port only direct connects
        print ("made flow")
        return zio.flow.Flow(port)

    node = zio.Node("dumper")
    server = node.port("server", zmq.SERVER)
    server.bind(server_address)
    node.online()

    actor = ZActor(ctx, handler, flow_maker)
    broker = FlowBroker(server, actor.pipe)

    cflow = flow_maker()
    cflow.send_bot(zio.Message(label=json.dumps(dict(direction='extract',credit=2))))
    ok = broker.poll(1000)      # client->handler
    assert(ok)
    ok = broker.poll(1000)      # handler->client
    assert(ok)
    msg = cflow.recv_bot(1000)
    assert(msg)
    print ("cflow got",msg)
    broker.stop()
    node.offline()
    

if '__main__' == __name__:
    test_dumper()
