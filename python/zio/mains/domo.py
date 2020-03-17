#!/usr/bin/env python3
import json
from .. import jsonnet
import click
from .. import rules
from .cli import cli

import logging
log = logging.getLogger("zio")

import zmq

sock_name2type = dict(router=zmq.ROUTER,
                      dealer=zmq.DEALER,
                      server=zmq.SERVER,
                      client=zmq.CLIENT)


@cli.command()
@click.option("--verbose/--no-verbose", default=False,
              help="Provide extra output")
@click.option("-n", "--number", default=10000,
              help="Number of requests")
@click.option("-f", "--frontend",
              type=click.Choice(["router", "server"], case_sensitive=False),
              default="router",
              help="Type name for the front-end socket")
@click.option("-b", "--backend",
              type=click.Choice(["router", "server"], case_sensitive=False),
              default="router",
              help="Type name for the back-end socket")
def tripping(verbose, number, frontend, backend):
    '''
    Run the tripping example.  The two optinal 
    '''
    from zio.domo.tripping import main

    fes_type = sock_name2type[frontend.lower()]
    bes_type = sock_name2type[backend.lower()]
    main(fes_type, bes_type, number, verbose)


@cli.command()
@click.option("--verbose/--no-verbose", default=False,
              help="Provide extra output")
@click.option("-s", "--socket",
              type=click.Choice(["router", "server"], case_sensitive=False),
              default="router",
              help="Type name for socket")
@click.option("-a","--address", default="tcp://127.0.0.1:5555")
def broker(verbose, socket, address):
    '''
    Run the domo broker
    '''
    from zio.domo.broker import Broker
    broker = Broker(sock_name2type[socket], verbose)
    broker.bind(address)
    broker.mediate()

@cli.command()
@click.option("--verbose/--no-verbose", default=False,
              help="Provide extra output")
@click.option("-s", "--socket",
              type=click.Choice(["dealer", "client"], case_sensitive=False),
              default="dealer",
              help="Type name for socket")
@click.option("-a","--address", default="tcp://127.0.0.1:5555")
def echo(verbose, socket, address):
    '''
    Run a worker providing an echo service
    '''
    from zio.domo.worker import Worker
    worker = Worker(address, b"echo", sock_name2type[socket], verbose)
    reply = None
    while True:
        request = worker.recv(reply)
        if request is None:
            break
        reply = request # Echo is complex... :D

@cli.command()
@click.option("--verbose/--no-verbose", default=False,
              help="Provide extra output")
@click.option("-s", "--socket",
              type=click.Choice(["dealer", "client"], case_sensitive=False),
              default="dealer",
              help="Type name for socket")
@click.option("-n", "--number", default=1,
              help="Number of times to repeat the service request")
@click.option("-a","--address", default="tcp://127.0.0.1:5555")
@click.argument("service")
@click.argument("args", nargs=-1)
def client(verbose, socket, number, address, service, args):
    '''
    Run a client providing an echo service
    '''
    from zio.domo.client import Client
    client = Client(address, sock_name2type[socket], verbose)

    service = service.encode('utf-8')
    request = [one.encode('utf-8') for one in args]
    for scount in range(number):
        try:
            if verbose:
                print("send",service,request)
            client.send(service, request)
        except KeyboardInterrupt:
            break
    for rcount in range(number):
        try:
            reply = client.recv()
        except KeyboardInterrupt:
            break
        if reply is None:
            break
        if verbose:
            print("recv",reply)

    print(f'{scount+1} sent, {rcount+1} recv')

