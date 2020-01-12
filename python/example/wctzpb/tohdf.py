#!/usr/bin/env python3
'''
Converters from PB to HDF
'''
import numpy
def Depo(depo, group):
    '''Write a Depo into an HDF group.

    The floating point numbers are put into a 1D array and the rest
    are added as attributes.
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

def is_dense(traces):
    '''Return (tbin,size) if traces are accidentally dense, else None.'''
    tbin = None
    size = None
    chans = set()
    for trace in traces:
        if tbin is None:
            tbin = trace.tbin
            size = len(trace.samples.elements)
            chans.add(trace.channel)
            continue
        if tbin != trace.tbin:
            return
        if size != len(trace.samples.elements):
            return
        if trace.channel in chans:
            return
        chans.add(trace.channel)
    return (tbin,size)

def save_dense(frame, group, tbin, nticks):
    '''
    Save traces as a dense 2D array
    '''
    nchans = len(frame.traces)
    chans = group.create_dataset("chans",(nchans,))
    samples = group.create_dataset("samples",(nchans,nticks), chunks=True)
    samples.attrs["tbin"] = tbin
    for ind,trace in enumerate(frame.traces):
        samples[ind,:] = trace.samples # does this broadcast work?
        chans[ind] = trace.channel
        
def save_sparse(frame, group):
    '''
    Save traces individually
    '''
    ntraces = len(frame.traces)
    chans = group.create_dataset("chans",(ntraces,), dtype='i')
    tbins = group.create_dataset("tbins",(ntraces,), dtype='i')
    tg = group.create_group("traces")
    for ind,trace in enumerate(frame.traces):
        nticks = len(trace.samples.elements)
        print(f"{ind} {nticks}")
        tbins[ind] = trace.tbin
        chans[ind] = trace.channel
        arr = numpy.asarray(trace.samples.elements, dtype='f')
        ds = tg.create_dataset(str(ind),data=arr)
        

def Frame(frame, group):
    '''Write a Frame into an HDF group.

    A frame is saved such that any ordering of traces is preserved.
    However, the trace samples will be saved in one of two modes:
    sparse or dense.

    The "dense" mode is used if the traces happen to perfectly span a
    rectangle in tick vs channel space.  This mode saves a 2D dataset
    named "samples" with an attrs named "tbin" holding the common tbin
    offset.  A trace charge array spans a row.  Rows are ordered
    following the original traces array.

    The "sparse" mode is used if traces represent a "ragged" array in
    tick vs channel space including if a channel has more than one
    trace.  The "sparse" mode saves traces in a subgroup named
    "traces".  Each trace charge array is saved as a 1D dataset named
    by its index into the original traces array.  The individual tbin
    offsets are stored in a 1D dataset named "tbins".

    Both modes save a 1D dataset named "chans" holding the channel
    array.  Trace tags are saved in a group called "tagged" as 1D
    datasets of trace indices using the tag name as the dataset name.
    Trace summaries are saved in a "summaries" group with each dataset
    taking the tag name.
    
    Note, the index of a summary dataset corresponds to an element in
    a tagged data set which gives the trace index.  A trace index can
    be used in the "chans" data set, as a row index for the "samples"
    data set or as a dataset name in the "traces" group.

    Frame tags are saved as keys in the top group attrs with a value
    of b'tag\0' (to distinguish from other top group attrs).

    If not empty, the channel mask map is stored in a group "cmm".
    Each channel mask is stored as a dataset with a name as its key in
    the cmm.  The dataset is 3xN integer array with rows of
    [chid,beg,end].  The chid entry may be repeated.

    '''
    group.attrs["ident"] = frame.ident
    group.attrs["time"] = frame.time
    group.attrs["tick"] = frame.tick

    for ftag in frame.frame_tags:
        group.attrs[ftag] = b'tag\0'

    ts = is_dense(frame.traces)
    if ts:
        save_dense(frame, group, *ts)
    else:
        save_sparse(frame, group)

    if len(frame.tagged_traces) > 0:
        g = group.create_group("tagged")
        for tag in frame.tagged_traces:
            indices = frame.tagged_traces[tag]
            ds = g.create_dataset(tag, (len(indices.elements),), dtype='i')
            for i2,ind in enumerate(indices.elements):
                ds[i2] = ind
    if len(frame.trace_summaries) > 0:
        g = group.create_group("summaries")
        for tag in frame.trace_summaries:
            values = frame.trace_summaries[tag]
            ds = g.create_dataset(tag, (len(values.elements),), dtype='f')
            for i2,val in enumerate(values.elements):
                ds[i2] = val
        
    if len(frame.channel_masks) > 0:
        g = group.create_group("cmm")
        for tag in frame.channel_masks:
            cms = frame.channel_masks[tag]
            size = 0
            for chid in cms.bin_range_lists:
                n = len(cms.bin_range_lists[chid].beg)
                size += n
                #print (f"{tag} {chid} {n} {size}")
            ds = g.create_dataset(tag, (size,3), dtype='i')
            ind = 0
            for chid in cms.bin_range_lists:
                rl = cms.bin_range_lists[chid]
                for i2 in range(len(rl.beg)):
                    ds[ind,0] = chid
                    ds[ind,1] = rl.beg[i2]
                    ds[ind,2] = rl.end[i2]
                    ind += 1

