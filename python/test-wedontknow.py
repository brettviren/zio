#!/usr/bin/env python3

import time
import pyre
p1 = pyre.Pyre("p1")
p1.start()
p2 = pyre.Pyre("p2")
p2.start()
# https://github.com/zeromq/pyre/issues/145
time.sleep(1)
p1.stop()
p2.stop()


