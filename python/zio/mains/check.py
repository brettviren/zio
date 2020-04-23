#!/usr/bin/env python3
'''
Some checks
'''

import click
import logging
from zio.util import modlog
log = modlog(__name__)

import zmq

@click.group("check")
@click.pass_context
def cli(ctx):
    '''
    ZIO command line interface checks
    '''

@cli.command('check-codec')
@click.option("-a","--address", default="tcp://127.0.0.1:5555")
@click.argument("socket-type")
def check_codec(address, socket_type):
    '''
    Run clientish (DEALER or CLIENT socket type) or serverish (ROUTER or SERVER) codec test.

    May be run against itself or against the C++ version test/check_codec.cpp.
    '''
    from zio.util import socket_names
    from zio.node import Node
    from zio.message import Message

    # match check_codec.cpp
    greeting = b"Hello World! "
    spaminess = 5
    def spamify():
        spam = greeting
        for count in range(spaminess):
            spam += spam
        return spam

    def serverish():
        node = Node("check-codec")
        port = node.port("server", socket_names.index(socket_type.upper()));
        port.bind(address)
        node.online()

        log.info("serverish receiving")
        msg = port.recv();
        log.info(msg)
        mp = msg.toparts()
        assert len(mp) == 4
        assert mp[2] == greeting
        assert mp[3] == spamify()

        log.info("serverish sending")
        port.send(msg);

    def clientish():
        node = Node("check-codec")
        port = node.port("client", socket_names.index(socket_type.upper()));
        port.connect(address)
        node.online()
        msg1 = Message(form="TEXT", payload=[greeting, spamify()]);
        log.info("clientish sending")
        port.send(msg1);
        log.info("clientish receiving")
        msg2 = port.recv();
        assert msg1.form == msg2.form
        mp1 = msg1.toparts()
        mp2 = msg2.toparts()
        assert len(mp1) == len(mp2)
        assert len(mp1) == 4
        for count, (p1,p2) in enumerate(zip(mp1, mp2)):
            assert len(p1) == len(p2)
            if count == 1:
                continue        # skip coord header as it should differ
            assert p1 == p2
            
    log.info(f'{socket_type} on {address}')

    if socket_type.lower() in ("server","router"):
        serverish()
    else:
        clientish()


    

