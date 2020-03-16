#!/usr/bin/env python3
'''
Support for reading HDF5 files.
'''

import json
import h5py
import numpy
from zmq import CLIENT
from ..util import message_to_dict
from zio import Port, Message
from zio.flow import Flow

import logging
log = logging.getLogger("zio")

class TensReader:
    '''Read ZIO TENS messages from HDF5

    This is the inverse of @ref writer.TensWriter.  See that class for
    details.

    '''

    # needs to match TensWriter
    seqno_interp = "%04d"
    part_interp = "%02d"

    def __init__(self, group):
        assert(group)
        self.group = group
        self.seqno = 0

    def read(self):
        'Read and return a message'
        # start at 1 because writer doesn't save anything for BOT
        self.seqno += 1         # fixme: should iterate seqnos over subgroups?
        gn = self.seqno_interp % self.seqno
        seq = self.group.get(gn)
        if not seq:
            log.error(f'TensReader: failed to get {gn} from {self.group}')
            return

        tens = seq.get("tensors")

        attrs = dict(tens.attrs)
        msg = Message(form='FLOW', 
                      origin = attrs.pop("origin"),
                      granule = attrs.pop("granule"),
                      seqno = attrs.pop("seqno"))
        for k,v in attrs.items():
            if type(v) == numpy.int64:
                attrs[k] = int(v)

        umd = seq.get("metadata")
        if umd:
            attrs["metadata"] = dict(umd.attrs)
        msg.label_object = attrs

        partnums = [int(p) for p in tens.keys()]
        ntens = len(partnums)
        maxpart = max(partnums)
        nparts = maxpart+1
        payload = [None]*nparts
        tensors = list()
        for part, ds in tens.items():
            part = int(part)
            # fixme there are more TENS attr which might be needed if
            # the file wasn't written by writer.TensWriter!
            md = dict(ds.attrs)
            dtype = str(ds.dtype)
            log.debug(f'TENS part {part}/{nparts} {dtype} {type(ds.dtype)} {ds.shape}')
            md.update(dict(
                shape = ds.shape,
                dtype = ds.dtype.kind,
                word = ds.dtype.alignment,
                part = part))
            tensors.append(md)
            payload[part] = ds[:].tostring()

        msg.payload = payload
        return msg


def handler(ctx, pipe, bot, rule_object, filename, broker_addr, *rargs):

    log.debug(f'actor: reader "{filename}"')
    fp = h5py.File(filename,'r')
    
    mattr = message_to_dict(bot)
    rattr = dict(rule_object.get("attr",{}), **mattr)
    base_path =  rule_object.get("grouppat","/").format(**rattr)
    log.debug(f'reader(msg, "{base_path}", "{broker_addr}")')
    log.debug(bot)
    pipe.signal()

    sock = ctx.socket(CLIENT)
    port = Port("read-handler", sock)
    port.connect(broker_addr)
    port.online(None)
    flow = Flow(port)
    log.debug (f'reader({base_path}) send BOT to {broker_addr}')

    sg = fp.get(base_path)
    if not sg:
        log.error(f'reader failed to get {base_path} from {filename}')
        return
    fr = TensReader(sg, *rargs)

    flow.send_bot(bot)          # this introduces us to the server
    bot = flow.recv_bot()
    log.debug (f'reader({base_path}) got response:\n{bot}')
    flow.slurp_pay()

    while True:
        msg = fr.read()
        log.debug(f'reader: {msg}')
        if not msg:
            break
        ok = flow.put(msg)
        if not ok:
            break;
    flow.send_eot()
    flow.recv_eot()
    
