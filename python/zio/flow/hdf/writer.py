#!/usr/bin/env python3
'''Support for writing HDF5 files.

It provides:

- a Writer class with a mapping policy from TENS message to HDF group

- a file handler actor function

- a data flow client handler actor function

'''

import json

from zio import Port, Message
from zio.flow import objectify, Flow
from pyre.zactor import ZActor
from zmq import CLIENT, PUSH, PULL, Poller, POLLIN
import h5py
import numpy
from ..util import message_to_dict

import logging
log = logging.getLogger("zio")


class TensWriter:
    '''Write ZIO TENS messages to HDF5.

    Like all flow writers to HDF5, this one will restrict its writing
    to a given base group.  

    It will make a subgroup for each message based on the message
    sequence number.

    '''
    
    seqno_interp = "%04d"
    part_interp = "%02d"

    def __init__(self, group):
        self.group = group

    def save(self, msg):
#        log.debug(f'save: {msg}')
        fobj = msg.label_object
        if fobj["flow"] == "BOT":
            return

        try:
            tens = fobj.pop("TENS")
            tensors = tens.pop("tensors")
        except KeyError:
            log.error('No TENS label attribute')
            log.error(f'{msg}')
            return
        user_md = tens.get("metadata",{})

        gn = self.seqno_interp % msg.seqno
        seq = self.group.get(gn, None)
        if seq is None:         # as it should be
            seq = self.group.create_group(gn)
        else:
            log.error(f'HDF5 TENS writer cowardly refusing to use existing group {gn}')
            log.error(f'{msg}')
            return
        tens = seq.create_group("tensors")

        tens.attrs["origin"] = msg.origin
        tens.attrs["granule"] = msg.granule

        for k,v in fobj.items():
            if k in ["direction"]:
                continue
            try:
                tens.attrs[k] = v
            except TypeError as te:
                ot = type(v)
                log.error(f'can not serialize type {ot} for key {k}')
                continue
                
        parts = msg.toparts()
        parts = parts[2:]       # skip headers
        nparts = len(parts)
        for tenind, tenmd in enumerate(tensors):
            part = int(tenmd.get('part', tenind))
            ten = parts[part]
            
            # required
            sword = str(tenmd['word'])
            shape = list(tenmd['shape'])
            dtype = str(tenmd['dtype'])

            log.debug(f'TENS PART: {tenind}/{nparts} {dtype} {sword} {shape}')

            data = numpy.frombuffer(ten, dtype=dtype+sword).reshape(shape)
            ds = tens.create_dataset(self.part_interp % part,
                                     data = data,
                                     chunks = True)

        if user_md:
            log.debug(f'writer: save with user md: {user_md}')
            md = seq.create_group("metadata")
            md.attrs.update(user_md)


def file_handler(ctx, pipe, filename, *wargs):
    '''An actor that marshals messages from socket to file.

    Parameters
    -----------

    filename : string

        Name of an HDF file in which to write

    wargs : tuple of args

    wargs[0] : string (address pattern)

        An f-string formatted with a "port" parameter that should
        resolve to a legal ZeroMQ socket address.  When a successful
        bind() can be done on the result, the resolved address is
        returned through the pipe.  If no successful address can be
        bound, an empty string is returned as an error indicator.

    '''
    wargs = list(wargs)
    addrpat = wargs.pop(0)
    log.debug(f'actor: writer("{filename}", "{addrpat}")')
    fp = h5py.File(filename,'w')
    log.debug(f'writer: opened {filename}')
    pipe.signal()
    pull = ctx.socket(PULL)
    minport,maxport = 49999,65000
    for port in range(minport,maxport):
        writer_addr = addrpat.format(port=port)
        pull.bind(writer_addr)
        log.debug(f'writer: bind to {writer_addr}')
        pipe.send_string(writer_addr)
        break

    flow_writer = dict()

    poller = Poller()
    poller.register(pipe, POLLIN)
    poller.register(pull, POLLIN)

    while True:
        for which,_ in poller.poll():

            if not which or which == pipe: # signal exit
                log.debug(f'writer: {filename} exiting')
                fp.close()
                return

            # o.w. we have flow

            data = pull.recv()
            if not data:
                continue
            msg = Message(encoded=data)
            fobj = objectify(msg)
            path = fobj.pop("hdfgroup") # must be supplied
            msg.label = json.dumps(fobj)
            log.debug(f'writer: {filename}:/{path} writing:\n{msg}')

            fw = flow_writer.get(path, None)
            if fw is None:
                sg = fp.get(path, None) or fp.create_group(path)
                # note: in principle/future, TensWriter type can be
                # made an arg to support other message formats.
                fw = flow_writer[path] = TensWriter(sg, *wargs)

            fw.save(msg)
            log.debug(f'writer: flush {filename}')
            fp.flush()
                
    return

def client_handler(ctx, pipe, bot, rule_object, writer_addr, broker_addr):
    '''Connect to and marshall messages between broker and writer sockets.

    Parameters
    ----------

    bot : zio.Message

        The BOT message

    rule_object: dicionary 

        A ruleset rule object.

    writer_addr :: string

        The address of the writer's PULL socket to connect.

    broker_addr : string

        The address of the broker's SERVER socket to connect.

    '''
    # An HDF path to be added to every message we send to writer.
    mattr = message_to_dict(bot)
    rattr = dict(rule_object.get("attr",{}), **mattr)
    log.info(f'writer: attrs: {rattr}')
    try:
        base_path =  rule_object.get("grouppat","/").format(**rattr)
    except KeyError as e:
        log.error(f'writer: missing attribute: {e}')
        raise
    log.debug(f'client_handler(msg, "{base_path}", "{broker_addr}", "{writer_addr}")')
    log.debug(bot)
    pipe.signal()

    push = ctx.socket(PUSH)
    push.connect(writer_addr)

    sock = ctx.socket(CLIENT)
    port = Port("write-handler", sock)
    port.connect(broker_addr)
    port.online(None)

    direction = mattr["direction"]
    if direction != "inject":
        raise RuntimeError(f'zio.flow.hdf.writer bad direction: "{direction}"')
    credit = mattr["credit"]
    flow = Flow(port, direction, credit)
    log.debug (f'writer({base_path}) send BOT to {broker_addr}')

    bot = flow.bot(bot)         # this introduces us to the server
    log.debug (f'writer({base_path}) got response:\n{bot}')
    flow.begin()

    def push_message(m):
        log.debug (f'write_handler({base_path}) push {m}')
        attr = message_to_dict(m)
        attr['hdfgroup'] = base_path
        m.label = json.dumps(attr)
        push.send(m.encode())

    push_message(bot)

    poller = Poller()
    poller.register(pipe, POLLIN)
    poller.register(sock, POLLIN)
    while True:

        for which,_ in poller.poll():
            if not which:
                return

            if which == pipe: # signal exit
                log.debug ('write_handler pipe hit')
                return          

            # o.w. we have flow

            try:
                msg = flow.get()
            except TransmissionEnd as te:
                flow.eotsend()
                push_message(te.msg)
                break
            continue

    log.debug ('write_handler exiting')
    pipe.signal()
    
