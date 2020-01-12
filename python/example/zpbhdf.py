#!/usr/bin/env python3
'''
Example zio/protobuf/hdf5 file server
'''

import zio
from collections import Counter
from zio.flow import objectify

from google.protobuf.any_pb2 import Any 

class Writer:
    '''Map ZIO messages to an HDF5 group.

    Each message is placed as zero or more HDF datasets in an HDF
    subgroup of the given group.  The relative subgroup name is formed
    as:

        <stream>/<counter>/<index>/<dataset>

    Path elements:
    ---------------
    stream : string
        The value from the `stream` attribute of the flow object
    counter : int
        Incremented each time stream is seen by the writer
    index : int
        Index into the message payload array
    dataset : names of one or more HDF5 datasets
        Determined by the converter

    The path elements may have attributes.

    If BOT message has a `stream` flow object attribute its attributes
    will be added to the <stream> group.

    The <counter> group will hold all attributes from the DAT message.

    The <dataset>s will have attributes depending on the converter.

    '''

    def __init__(self, group, pbmod, tohdfmod):
        self.group = group
        self.counter = Counter()        
        self.pbmod = pbmod
        self.tohdf = tohdfmod

    def make_group(self, stream):
        gn = self.counter[stream]
        self.counter[stream] += 1
        return self.group.create_group(f"{stream}/{gn}")

    def save(self, msg):
        fobj = objectify(msg)
        stream = fobj["stream"]
        group = self.make_group(stream)
        for k,v in fobj.items():
            group.attrs[k] = v

        for index, data in enumerate(msg.payload):
            print (data)
            a = Any()
            size = a.ParseFromString(data)
            assert(size>0)
            tn = a.TypeName()
            print (tn)
            type_name = tn.rsplit('.',1)[-1]
            PBType = getattr(self.pbmod, type_name)
            pbobj = PBType()
            ok = a.Unpack(pbobj)
            assert(ok)
            tohdf = getattr(self.tohdf, type_name)
            slot = group.create_group(str(index))
            tohdf(pbobj, slot)

