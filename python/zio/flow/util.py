#!/usr/bin/env python3

import json

def objectify(morl):
    '''
    Return a flow object.

    The morl may be a zio.Message or a zio.Message.label
    '''
    if not morl:
        return dict()
    if type(morl) is bytes:
        morl = morl.decode('utf-8')
    if type(morl) is str:
        return json.loads(morl)
    return objectify(morl.prefix.label)

def stringify(flowtype, **params):
    '''
    Return a flow label string of given flow type and any extra
    parameters.
    '''
    params = params or dict()
    params['flow'] = flowtype
    return json.dumps(params)


def switch_direction(fobj):
    if isinstance(fobj, str):
        fobj = json.loads(fobj)
    else:
        fobj = dict(fobj)
    if fobj["direction"] == 'inject':
        fobj["direction"] = 'extract'
    elif fobj["direction"] == 'extract':
        fobj["direction"] = 'inject'
    else:
        raise KeyError('direction')
    return fobj

        
