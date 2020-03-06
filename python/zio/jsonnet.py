#!/usr/bin/env python3

import os
import json
import _jsonnet

def try_path(path, rel):
    if not rel:
        raise RuntimeError('Got invalid filename (empty string).')
    if rel[0] == '/':
        full_path = rel
    else:
        full_path = os.path.join(path, rel)
    if full_path[-1] == '/':
        raise RuntimeError('Attempted to import a directory')

    if not os.path.isfile(full_path):
        return full_path, None
    with open(full_path) as f:
        return full_path, f.read()

def import_callback(path, rel):
    paths = [path] + os.environ.get("WIRECELL_PATH","").split(":")
    paths += os.environ.get("JSONNET_PATH","").split(":")
    for maybe in paths:
        if not maybe:
            continue
        try:
            full_path, content = try_path(maybe, rel)
        except RuntimeError:
            continue
        if content:
            return full_path, content
    raise RuntimeError('File not found')


def load(fname):
    '''
    Load and evaluate a jsonnet file returning data structure.
    '''
    text = _jsonnet.evaluate_file(fname, import_callback=import_callback)
    return json.loads(text)

    
