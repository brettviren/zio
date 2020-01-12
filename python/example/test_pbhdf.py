#!/usr/bin/env python3

import zio
import h5py
import json    
from zpbhdf import Writer

from wctzpb import pb, tohdf

from google.protobuf.any_pb2 import Any 

if '__main__' == __name__:

     # Here we pretend to be some remote source of ZIO messages
     depo = pb.Depo(pos=pb.Point(x=1,y=2,z=3),
                    time=100.0, charge=1000.0) 
     from google.protobuf.any_pb2 import Any
     a = Any()
     a.Pack(depo)
     # pl = pb.Payload()
     # pl.objects.append(a)


     msg = zio.Message(form='FLOW',
                       label=json.dumps({'stream':'depos'}),
                       payload=[a.SerializeToString()])

     #
     # Wave our hands hiding all the ZIO networking that would
     # intervene now.  Skip the broker/backend, and cut to the end.
     #


     fp = h5py.File("hpbhdf.hdf",'w')
     base = fp.create_group("base")
     writer = Writer(base, pb, tohdf)
     writer.save(msg)
     
