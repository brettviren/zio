"""Majordomo Protocol Client API, Python version.

Implements the MDP/Worker spec at http:#rfc.zeromq.org/spec:7.

Author: Min RK <benjaminrk@gmail.com>
Based on Java example by Arkadiusz Orzechowski

Originally from Zguide examples, generalized to use CLIENT 
or DEALER by brett.viren@gmail.com
"""

import logging

import zmq

from . import MDP
from .zhelpers import dump, clientish_recv, clientish_send

class Client(object):
    """Majordomo Protocol Client API, Python version.

      Implements the MDP/Worker spec at http:#rfc.zeromq.org/spec:7.
    """
    broker = None
    ctx = None
    client = None
    poller = None
    timeout = 2500
    verbose = False

    def __init__(self, broker, socket_type=zmq.ROUTER, verbose=False):
        self.broker = broker
        self.socket_type = socket_type
        self.verbose = verbose
        self.ctx = zmq.Context()
        self.poller = zmq.Poller()
        logging.basicConfig(format="%(asctime)s %(message)s",
                            datefmt="%Y-%m-%d %H:%M:%S",
                            level=logging.INFO)
        self.reconnect_to_broker()


    def reconnect_to_broker(self):
        """Connect or reconnect to broker"""
        if self.client:
            self.poller.unregister(self.client)
            self.client.close()
        self.client = self.ctx.socket(self.socket_type)
        self.client.linger = 0
        self.client.connect(self.broker)
        self.poller.register(self.client, zmq.POLLIN)
        if self.verbose:
            logging.info("I: connecting to broker at %s...", self.broker)

    def send(self, service, request):
        """Send request to broker
        """
        if not isinstance(request, list):
            request = [request]

        # Prefix request with protocol frames
        # Frame 0: "MDPCxy" (six bytes, MDP/Client x.y)
        # Frame 1: Service name (printable string)

        request = [MDP.C_CLIENT, service] + request
        if self.verbose:
            logging.warn("I: send request to '%s' service: ", service)
            dump(request)
        #self.client.send_multipart(request)
        clientish_send(self.client, request)

    def recv(self):
        """Returns the reply message or None if there was no reply."""
        try:
            items = self.poller.poll(self.timeout)
        except KeyboardInterrupt:
            return # interrupted

        if items:
            # if we got a reply, process it
            #msg = self.client.recv_multipart()
            msg = clientish_recv(self.client)
            if self.verbose:
                logging.info("I: received reply:")
                dump(msg)

            # Don't try to handle errors, just assert noisily
            assert len(msg) >= 4

            header = msg.pop(0)
            assert MDP.C_CLIENT == header

            service = msg.pop(0)
            return msg
        else:
            logging.warn("W: permanent error, abandoning request")
