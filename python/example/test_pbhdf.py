#!/usr/bin/env python3

import zio
import h5py
import json    
import time
import random
import numpy
from zpbhdf import Writer

from wctzpb import pb, tohdf

from google.protobuf.any_pb2 import Any 

if '__main__' == __name__:

    # In a "real" app, this would live off in a server.  The group
    # name would be chosen based on some unique identifier for the
    # flow.
    fp = h5py.File("hpbhdf.hdf",'w')
    base = fp.create_group("unique/flow/id")
    writer = Writer(base, pb, tohdf)


    # Here we pretend to be some remote source of ZIO messages
    depo = pb.Depo(ident=99,
                   pos=pb.Point(x=1,y=2,z=3),
                   time=100.0, charge=1000.0,
                   trackid=12345, pdg=11,
                   extent_long=9.6, extent_tran=6.9) 

    a = Any()
    a.Pack(depo)
    # pl = pb.Payload()
    # pl.objects.append(a)
    
    msg = zio.Message(form='FLOW',
                      label=json.dumps({'stream':'depos'}),
                      payload=[a.SerializeToString()])
    msg.seqno = 42

    #
    # Normally, msg is created in remote client, sent over ZIO flow to
    # some server and dispatched to a matching writer.  Wave our hands
    # hiding all that and directly save.
    #
    
    writer.save(msg)
    
    #
    # Normally, one writer would service one type of stream, but in
    # this test we'll force it to also save a frame.  This test
    # should use the "sparse" frame save mode.
    #

    frame = pb.Frame(ident=2, time=time.time(), tick=500.0)
    chanset = list(range(1000))
    negs = frame.tagged_traces["negs"].elements
    negs_tot = frame.trace_summaries["negs"].elements
    for ind in range(500):
        chan = random.choice(chanset)
        chanset.remove(chan)
        tbin = int(random.uniform(0,1000))
        nticks = int(random.uniform(tbin,1000))
        if nticks == 0:
            nticks = 1
            tbin -= 1
        samples = numpy.random.normal(size=nticks)
        tr = pb.Trace(channel=chan, tbin=tbin)
        for s in samples:
            tr.samples.elements.append(s)
        frame.traces.append(tr)
        tot = numpy.sum(samples)
        if tot < 0:
            negs.append(ind)
            negs_tot.append(tot)
            brl = frame.channel_masks["bad"].bin_range_lists[chan]
            brl.beg.append(tbin)
            brl.end.append(tbin+nticks)

    if len(negs) > 0:
        frame.frame_tags.append("negged")
                      
    a = Any()
    a.Pack(frame)
    msg = zio.Message(form='FLOW',
                      label=json.dumps({'stream':'frames'}),
                      payload=[a.SerializeToString()])
    msg.seqno = 43
    writer.save(msg)
     

    # 
    # Do it again with an accidentally rectangular frame
    #
    frame = pb.Frame(ident=3, time=time.time(), tick=500.0)
    nchans = 500
    nticks = 1000
    samples = numpy.random.normal(size=(nchans,nticks))
    chanset = list(range(1000))
    tbin=0
    for ind in range(500):
        chan = random.choice(chanset)
        chanset.remove(chan)
        tr = pb.Trace(channel = chan, tbin=tbin)
        for s in samples[ind]:
            tr.samples.elements.append(s)
        frame.traces.append(tr)
        tot = numpy.sum(samples[ind])
        if tot < 0:
            negs.append(ind)
            negs_tot.append(tot)
            brl = frame.channel_masks["bad"].bin_range_lists[chan]
            brl.beg.append(tbin)
            brl.end.append(tbin+nticks)
    if len(negs) > 0:
        frame.frame_tags.append("negged")
                      
    a = Any()
    a.Pack(frame)
    msg = zio.Message(form='FLOW',
                      label=json.dumps({'stream':'frames'}),
                      payload=[a.SerializeToString()])
    msg.seqno = 44
    writer.save(msg)
        
