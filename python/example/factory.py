#!/usr/bin/env python
'''
A flow broker factory for providing an HDF file service.

It applies a rule set to each BOT and constructs a matching actor.

For client extraction flows, the actors simply forward messages to
their PUSH and the connected PULL in a writing actor writes to file.

For client inject flows, .... something else happens.  TBD.
'''

def message_to_dict(msg):
    '''
    Return a simple dictionary of message info
    '''
    d = objectify(msg)
    d['origin'] = msg.origin
    d['granule'] = msg.granule
    d['seqno'] = msg.seqno
    return d

def writer(ctx, pipe, filename, base, grouppat, writer_addr):
    pipe.signal()

    pull = ctx.socket(zmq.PULL)
    pull.bind(writer_addr)

    fp = h5py.File("hpbhdf.hdf",'w')
    base = fp.create_group(base)
    base.addr["zmqaddr"] = writer_addr

    flow_writer = dict()

    while True:
        data = pull.recv()
        if not data:
            continue
        msg = zio.Message(encoded=data)
        attr = message_to_dict(msg)
        path = grouppat.format(attr)
        ftype = attr["flow"]

        try:
            fw = flow_writer[path]
        except KeyError:
            sg = base.create_group(path)
            fw = flow_writer[path] = Writer(sg, pb, tohdf)

        fw.save(msg)
        

    return

def write_handler(ctx, pipe, me, bot, broker_addr, writer_addr, *args):
    '''
    Connect to and marshall messages between broker SERVER and writer
    PUSH.
    '''
    pipe.signal()
    client = ctx.socket(zmq.CLIENT)
    client.connect(broker_addr)
    push = ctx.socket(zmq.PUSH)
    push.connect(writer_addr)

    port = zio.Port("write-handler", zmq.CLIENT)
    flow = zio.Flow(port)
    flow.send_bot(bot)          # this introduces us to the server
    flow.recv_bot()

    push.send(bot.encode())

    while True:
        msg = flow.recv()
        if not msg:
            flow.send_eot()
            # fixme: send an EOT also to push.
            break

        fobj = objectify(msg)
        if not fobj.get(form, None):
            continue

        ftype = fobj.get("flow", None)
        if not ftype:
            continue

        # fixme: maybe add some to fobj here and repack

        push.send(msg.encode())

        if ftype == 'EOT':
            flow.send_eot()
            
        # fixme: we might get fresh BOT, check for it.

        continue
    

class Factory:

    def __init__(self, ruleset):
        '''
        Create a Factory with a ruleset.

        
        '''
        self.ruleset = ruleset
        pass

    def __call__(self, bot):
        '''
        Given a bot, return a live actor or None if bot is rejected.
        '''
        

        return
    
