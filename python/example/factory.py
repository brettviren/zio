#!/usr/bin/env python
'''
A flow broker factory for providing an HDF file service.

It applies a rule set to each BOT and constructs a matching actor.

For client extraction flows, the actors simply forward messages to
their PUSH and the connected PULL in a writing actor writes to file.

For client inject flows, .... something else happens.  TBD.
'''

from rule import Rule           # bv's hacked version
import lispish

def message_to_dict(msg):
    '''
    Return a simple dictionary of message header info.
    '''
    d = objectify(msg)
    d['origin'] = msg.origin
    d['granule'] = msg.granule
    d['seqno'] = msg.seqno
    return d

def writer(ctx, pipe, filename, base, grouppat, addrpat):
    '''An actor that marshals messages from socket to file.

    Parameters
    -----------

    filename : string

        Name of an HDF file in which to write

    base : string

        Relative path in the file to which all messages are written.

    groupat : string

        F-string pattern resolved against message attributes to
        produce a further per-message subpath.

    '''
    pipe.signal()

    pull = ctx.socket(zmq.PULL)
    minport,maxport = 49999,65000
    for port in range(minport,maxport):
        writer_addr = addrpat.format(port=port)
        got = pull.bind(writer_addr)
        if got == port:
            pipe.send_string(writer_addr)
            break
    if port==maxport:
        pipe.send_str("")
        return


    fp = h5py.File(filename,'w')
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

def write_handler(ctx, pipe, bot, msgpat, broker_addr, writer_addr, *args):
    '''Connect to and marshall messages between broker and writer.

    Parameters
    ----------

    bot : zio.Message

        The BOT message

    msgpat : f-string

        Formatted against each message header attributes to determine
    an added "stream" flow object attribute used to finally locate
    data in file.

    broker_addr : string

        The address of the broker's SERVER socket to connect.

    writer_addr :: string

        The address of the writer's PULL socket to connect.

    '''
    pipe.signal()

    port = zio.Port("write-handler", zmq.CLIENT)
    port.connect(broker_addr)
    port.onlien(None)
    flow = zio.Flow(port)
    flow.send_bot(bot)          # this introduces us to the server
    flow.recv_bot()

    def set_stream(m):
        attr = message_to_dict(m)
        stream = msgpat.format(**attr)
        attr['stream'] = stream
        m.label = json.dumps(attr)

    push = ctx.socket(zmq.PUSH)
    push.connect(writer_addr)
    set_stream(bot)
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

        set_stream(msg)
        push.send(msg.encode())

        if ftype == 'EOT':
            flow.send_eot()
            
        # fixme: we might get fresh BOT, check for it.

        # fixme: how do we exit?
        continue
    

class Factory:

    def __init__(self, ruleset, broker_addr, addrpat):
        '''Create a Factory with a ruleset.

        Parameters
        ----------

        addrpat : string

            F-string pattern formated with a "port" to produce a
        ZeroMQ address to bind.

        broker_addr : string

            A ZeroMQ address string to which handlers should connect.


        ruleset: array of dictionary

            A ruleset is a sequence of rule specifications, each of
        which is a dictionary with these attributes:

        Attributes
        ----------

        rule : string

            An S-expression in terms of attributes evaluating to a
        Boolean.  First rule returning True is used.

        rw : string 

            State if we should "read" or "write" this and future
        messages from/to file.

        filepat : f-string

            An f-string formatted against message attributes which
        gives the filename to read or write.

        msgpat : f-string

            An f-string formatted against message attributes which
        gives the relative subgroup in which to read/write data from
        this and subsequent messages from the same client.

        attr : dictionary
        
            An extra dictionary of attributes updated on the
        dictionary of message attributes prior to applying to the rule
        or either patterns.

        '''
        self.ruleset = list()
        sexp = lispish.sexp()
        for one in ruleset:
            p = sexp(one['rule'])
            r = Rule(p)
            self.ruleset.append(dict(one, check=r.match))
        self.broker_addr = broker_addr
        self.writers = dict()
        return

    def __call__(self, bot):
        '''
        Given a bot, return a live actor or None if bot is rejected.
        '''
        attr = message_to_dict(bot)
        
        for maybe in self.ruleset:
            rattr = dict(attr, **maybe["attr"])
            if not maybe["check"](rattr):
                continue
            filename = maybe["filepat"].format(**rattr)


            if maybe["rw"].startswith('r'):
                continue        # fixme: we only support writing so far
            # else: write

            # just in time construction of writer for a given file
            try:
                wactor = self.writers["filename"]
            except KeyError:
                wactor = ZActor(self.ctx, writer,
                                filename, base, grouppat, addrpat)
                waddr = wactor.recv_string()
                if not waddr:
                    raise RuntimeError(f"failed to bind any {addrpat} for {filename")
                wactor.addr = waddr # copascetic?
                self.writers["filename"] = wactor

            actor = ZActor(self.ctx, write_handler, bot, maybe["msgpat"],
                           self.broker_addr,
                           self.writers[filename].addr)
            return actor
        return
    
