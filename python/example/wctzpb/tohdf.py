#!/usr/bin/env python3
'''
Converters from PB to HDF
'''

def Depo(depo, group):
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
