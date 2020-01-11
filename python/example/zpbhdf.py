#!/usr/bin/env python3
'''
Example zio/protobuf/hdf5 file server
'''

import zio
import h5py
import json
import wctzpb_pb2 as wctzpb
from collections import Counter
from zio.flow import objectify

class Writer:
    '''
    Map ZIO messages to an HDF5 group.

    Each message is placed as zero or more HDF datasets in an HDF
    subgroup of the given group.  The relative subgroup name is formed
    as:

    <stream>/<counter>/

    With <stream> taken from the flow object and counter being a
    per-stream count maintained by the writer.
    '''

    counter_format = "%06d"

    def __init__(self, group, writer):
        self.writer = writer
        self.group = group
        self.counter = Counter()        

    def make_group(self, stream):
        gn = self.counter_format % self.counter[stream]
        self.counter[stream] += 1
        return self.group.create_group("%s/%s" % (stream,gn))

    def save(self, msg):
        fobj = objectify(msg)
        stream = fobj["stream"]
        g = self.make_group(stream)
        for k,v in fobj.items():
            g.attrs[k] = v
        self.writer(g,msg)
        

def wctzpb_write_Depo(group, depo):
    '''
    Write a Depo into an hdf group
    '''
    ds = group.create_dataset("depo", (8,), dtype='f')
    pos = depo.pos
    ds[0] = pos.x
    ds[1] = pos.y
    ds[2] = pos.z
    ds[3] = depo.time
    ds[4] = depo.charge
    ds[5] = depo.energy
    ds[6] = depo.extent_long
    ds[7] = depo.extent_tran    
    group.attrs["ident"] = depo.ident
    group.attrs["trackid"] = depo.trackid
    group.attrs["pdg"] = depo.pdg
    

def wctzpb_writer(group, msg):
    data = msg.payload[0]
    pl = wctzpb.Payload()
    pl.ParseFromString(data)
    a = pl.objects[0]
    tn = a.TypeName()
    type_name = tn.split('.')[1]
    Type = getattr(wctzpb, type_name)
    obj = Type()
    a.Unpack(obj)
    methname = f"wctzpb_write_{type_name}"
    meth = eval(methname)
    meth(group, obj)

    

if '__main__' == __name__:
     fp = h5py.File("hpbhdf.hdf",'w')
     base = fp.create_group("base")
     writer = Writer(base, wctzpb_writer)

     depo = wctzpb.Depo(pos=wctzpb.Point(x=1,y=2,z=3),
                        time=100.0, charge=1000.0) 
     from google.protobuf.any_pb2 import Any
     a = Any()
     a.Pack(depo)
     
     pl = wctzpb.Payload()
     pl.objects.append(a)
     msg = zio.Message(form='FLOW',
                       label=json.dumps({'stream':'depos'}),
                       payload=[pl.SerializeToString()])
     writer.save(msg)
     
