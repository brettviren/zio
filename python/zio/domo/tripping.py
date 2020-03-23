"""Round-trip demonstrator

While this example runs in a single process, that is just to make
it easier to start and stop the example. Client thread signals to
main when it's ready.

Originally from Zguide examples, generalized to use CLIENT/SERVER by
brett.viren@gmail.com

"""

import sys
import threading
import time

import zmq
from zio.util import zpipe, 
from zio.util import serverish_recv, serverish_send,
from zio.util import clientish_recv, clientish_send


def client_task (ctx, pipe, stype, requests=10000, verbose=False):
    client = ctx.socket(stype)
    client.identity = b'C'
    client.connect("tcp://localhost:5555")

    print (f"Client with socket {stype}...")
    time.sleep(0.5)

    print ("Synchronous round-trip test...")
    start = time.time()
    for r in range(requests):
        clientish_send(client, f'hello s {r}'.encode('utf-8'))
        #client.send(f'hello s {r}'.encode('utf-8'))
        msg = clientish_recv(client)[0].decode('utf-8')
        #msg = client.recv().decode('utf-8')
        #print (f'->sc {msg}')
        parts = msg.split()
        assert(len(parts) == 3)
        assert(parts[0] == 'hello')
        assert(parts[1] == 's')
        assert(parts[2] == str(r))
    print (" %d calls/second" % (requests / (time.time()-start)))

    print ("Asynchronous round-trip test...")
    start = time.time()
    for r in range(requests):
        clientish_send(client, f'hello a {r}'.encode('utf-8'))
    for r in range(requests):
        msg = clientish_recv(client)[0].decode('utf-8')
        #print (f'->ac {msg}')
        parts = msg.split()
        assert(len(parts) == 3)
        assert(parts[0] == 'hello')
        assert(parts[1] == 'a')
        assert(parts[2] == str(r))
    print (" %d calls/second" % (requests / (time.time()-start)))

    # signal done:
    pipe.send(b"done")

def worker_task(stype, verbose=False):
    ctx = zmq.Context()
    worker = ctx.socket(stype)
    worker.identity = b'W'
    worker.connect("tcp://localhost:5556")
    time.sleep(0.5)
    print (f"Worker with socket {stype}...")
    clientish_send(worker, b"greetings")
    while True:
        msg = clientish_recv(worker)
        clientish_send(worker, msg)
    ctx.destroy(0)

def broker_task(fes_type, bes_type, verbose=False):
    # Prepare our context and sockets
    ctx = zmq.Context()
    frontend = ctx.socket(fes_type)
    backend = ctx.socket(bes_type)
    frontend.bind("tcp://*:5555")
    backend.bind("tcp://*:5556")
    print (f"Broker with sockets {fes_type} <--> {bes_type}")

    # Initialize poll set
    poller = zmq.Poller()
    poller.register(backend, zmq.POLLIN)
    poller.register(frontend, zmq.POLLIN)

    feid = beid = None

    beid, msg = serverish_recv(backend)
    print (msg)
    assert(msg[0] == b"greetings")

    fe_waiting = list()
    be_waiting = list()

    while True:
        try:
            items = dict(poller.poll())
        except:
            break # Interrupted

        if frontend in items:
            feid,msg = serverish_recv(frontend)
            be_waiting.append(msg)

        if backend in items:
            beid, msg = serverish_recv(backend)
            fe_waiting.append(msg)

        if feid:
            for msg in fe_waiting:
                #print(f'b->f {msg}')
                serverish_send(frontend, feid, msg)
            fe_waiting.clear()
                    
        if beid:
            for msg in be_waiting:
                #print(f'f->b {msg}')
                serverish_send(backend, beid, msg)
            be_waiting.clear()

def main(fes_type=zmq.ROUTER, bes_type=zmq.ROUTER,
         requests = 10000, verbose=False):
    otype = {zmq.ROUTER:zmq.DEALER, zmq.SERVER:zmq.CLIENT}
    fec_type = otype[fes_type]
    bec_type = otype[bes_type]

    # Create threads
    ctx = zmq.Context()
    client,pipe = zpipe(ctx)

    client_thread = threading.Thread(target=client_task,
                                     args=(ctx, pipe, fec_type, requests, verbose))
    worker_thread = threading.Thread(target=worker_task,
                                     args=(bec_type,verbose))
    worker_thread.daemon=True
    broker_thread = threading.Thread(target=broker_task,
                                     args=(fes_type, bes_type, verbose))
    broker_thread.daemon=True

    worker_thread.start()
    broker_thread.start()
    client_thread.start()

    # Wait for signal on client pipe
    client.recv()

if __name__ == '__main__':
    main()
