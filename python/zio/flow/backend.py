#!/usr/bin/env python3
'''
Actor functions that can work as broker backends.
'''
import zmq
from ..message import Message

import logging
log = logging.getLogger(__name__)

def spawner(ctx, pipe, factory, *args):
    '''An actor that spawns other actors.

    Parameters
    ----------
    factory : a callable
        Called with new BOT, return actor or None if reject.

    '''
    log.debug(f'actor: spawner({factory})')
    poller = zmq.Poller()
    poller.register(pipe, zmq.POLLIN)
    pipe.signal()               # ready

    actors = dict()             # sock to zactor

    terminated = False
    while not terminated:

        for sock,_ in poller.poll():

            if sock == pipe:
                data = pipe.recv()

                if len(data) == 0:
                    log.debug("spawner got EOT")
                    terminated = True
                    break
                if data == b'STOP':
                    log.debug("spawner got STOP")
                    terminated = True
                    break

                bot = Message(encoded=data)
                log.debug (f'spawner:\n{bot}')
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
                log.debug ("spawner with %d actors spawned" % len(actors))

    if actors:
        log.debug("spawner killing %d actors" % len(actors))
        for pipe,actor in actors.items():
            pipe.signal()
    log.debug ("spawner exiting")
    
    return
