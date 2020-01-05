#!/usr/bin/env python3

import zio
import time

p1 = zio.Peer("p1", a='42', b='hello')

p2 = zio.Peer("p2", a='6.9', b='world')

time.sleep(1)
print ("draining")
p1.drain()
p2.drain()

print (p1.peers)

print("done")

# p1.stop()
del p1

time.sleep(1)
p2.drain()
time.sleep(1)

p2.stop()

