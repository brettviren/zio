#!/usr/bin/env python3
'''
Example zio/protobuf/hdf5 file server
'''

import zio
from zio.flow import objectify

from google.protobuf.any_pb2 import Any 
import logging
log = logging.getLogger(__name__)
class Writer:
    '''Write ZIO messages to HDF5.

    The writer is given a base group into which data from subsequent
    messages are written.  

    The message information is placed in a subgroup:

        <seqno>/<index>/[<datasets>]

    Path elements:
    ---------------
    seqno : int
        The seqno attribute from the message coord header.
    index : int
        Zero-based index into the message payload array

    The `<seqno>` group holds as HDF `attrs` the flow object (prefix
    header label) as well as `origin` and `granule` values from the
    coord header.  

    Zero or more datasets may be defined to hold array-like
    information and they may have HDF `attrs` set.
    '''

    def __init__(self, group, pbmod, tohdfmod):
        self.group = group
        self.pbmod = pbmod
        self.tohdf = tohdfmod

    def save(self, msg):
        fobj = objectify(msg)
        gn = str(msg.seqno)
        seq = self.group.get(gn, None)
        if seq is None:         # as it should be
            seq = self.group.create_group(gn)
        else:
            log.warning(f'HDF5 writer reusing existing group {gn}')
        seq.attrs["origin"] = msg.origin
        seq.attrs["granule"] = msg.granule
        for k,v in fobj.items():
            if k in ["flow", "direction"]:
                continue
            seq.attrs[k] = v
        
        for index, data in enumerate(msg.payload):
            a = Any()
            size = a.ParseFromString(data)
            assert(size>0)
            tn = a.TypeName()
            log.debug(f"payload {index} of size {size} and type {tn}")
            type_name = tn.rsplit('.',1)[-1]
            PBType = getattr(self.pbmod, type_name)
            pbobj = PBType()
            ok = a.Unpack(pbobj)
            if not ok:
                err = f'fail to unpack protobuf object of type {tn}'
                log.error(err)
                raise RuntimeError(err)
            tohdf = getattr(self.tohdf, type_name)
            slot = seq.create_group(str(index))
            tohdf(pbobj, slot)

