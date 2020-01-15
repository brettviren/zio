#!/usr/bin/env python3
'''
Actor functions that can work as broker backends.
'''
import zmq
from ..message import Message

def spawner(ctx, pipe, factory, *args):
    '''An actor that spawns other actors.

    Parameters
    ----------
    factory : a callable
        Called with new BOT, return actor or None if reject.

    '''
    print(f'actor: spawner({factory})')
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    actors = dict()             # sock to zactor

    terminated = False
    while not terminated:

        for sock,_ in poller.poll():

            if sock == pipe:
                data = pipe.recv()
                print ("spawner: data:", data)
                if len(data) == 0:
                    print("spawner got EOT")
                    terminated = True
                    break
                if data == b'STOP':
                    print("spawner got STOP")
                    terminated = True
                    break

                bot = Message(encoded=data)
                actor = factory(bot)
                if actor is None:
                    pipe.send_string('NO')
                    continue
                actors[actor.pipe] = actor
                poller.register(actor.pipe, zmq.POLLIN)
                pipe.send_string('OK')
                continue
            else:
                # else its a spawned actor signaling us
                poller.unregister(sock)
                del actors[sock]
                print ("spawner with %d actors spawned" % len(actors))

    if actors:
        print("spawner with %d live actors" % len(actors))
        # now kill them
    print ("spawner exiting")
    
    return
