#!/usr/bin/env python3
'''
The zio module provides Python functionality similar to C++ libzio.

This module does not bepend on libzio and instead uses PyZMQ and Pyre.

The Python classes are similar if not exactly the same as those found
in the zio:: C++ namespace.

>>> import zio
>>> n = zio.Node(...)
>>> p = n.port("portname", zmq.CLIENT)
>>> p.bind(...)
>>> p.connect(...)
>>> f = zio.Flow(p)
>>> bot = f.bot()
>>> dat = ...
>>> ok = f.dat(dat)
'''

# emulate libzio's namespace/class tree 
from .message import *
from .port import *
from .node import *
from .peer import *
from . import flow

