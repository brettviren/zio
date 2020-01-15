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

def writer(ctx, pipe, filename, addrpat):
    '''An actor that marshals messages from socket to file.

    Parameters
    -----------

    filename : string

        Name of an HDF file in which to write

    addrpat : string

        An f-string formatted with a "port" parameter that should
        resolve to a legal ZeroMQ socket address.  When a successful
        bind() can be done on the result, the resolved address is
        returned through the pipe.  If no successful address can be
        bound, an empty string is returned as an error indicator.

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

    flow_writer = dict()

    while True:
        data = pull.recv()
        if not data:
            continue
        msg = zio.Message(encoded=data)
        attr = message_to_dict(msg)
        path = attr["hdfgroup"] # must be supplied

        try:
            fw = flow_writer[path]
        except KeyError:
            sg = fp.create_group(path)
            fw = flow_writer[path] = Writer(sg, pb, tohdf)

        fw.save(msg)
        

    return

def write_handler(ctx, pipe, bot, base_path, broker_addr, writer_addr):
    '''Connect to and marshall messages between broker and writer.

    Parameters
    ----------

    bot : zio.Message

        The BOT message

    base_path : string

        An HDF path to be added to every message we send to writer.

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


    push = ctx.socket(zmq.PUSH)
    push.connect(writer_addr)

    def push_message(m):
        attr = message_to_dict(m)
        attr['hdfgroup'] = base_path
        m.label = json.dumps(attr)
        push.send(m.encode())

    push_message(bot)

    while True:
        msg = flow.recv()
        if not msg:
            flow.send_eot()
            # fixme: send an EOT also to push socket?.
            break

        fobj = objectify(msg)
        if not fobj.get(form, None):
            continue

        ftype = fobj.get("flow", None)
        if not ftype:
            continue

        # fixme: maybe add some to fobj here and repack

        push_message(msg)

        if ftype == 'EOT':
            flow.send_eot()
            
        # fixme: we might get fresh BOT, check for it.

        # fixme: how do we exit?
        continue
    

class Factory:

    def __init__(self, ruleset, broker_addr,
                 addrpat = "tcp://127.0.0.1:{port}"):
        '''Create a Factory with a ruleset.

        Parameters
        ----------

        addrpat : string

            F-string pattern formated with a "port" to produce a
            ZeroMQ address to bind.  Used by writer or reader actors.

        broker_addr : string

            A ZeroMQ address string of the broker to which handlers
            should connect.

        ruleset: array of dictionary

            A ruleset is a sequence of rule specifications, each of
            which is a dictionary with these attributes:

        Rule Attributes
        ---------------

        rule : string

            An S-expression in terms of attributes evaluating to a
            Boolean.  First rule returning True is used.  Here and
            below "attributes" refer to the combination of BOT message
            header values and any additional specified in the `attr`
            Rule Attribute (described below).

        rw : string 

            Takes value "read" or "write" and determines if the flow
            is to be read from or written to file.

        filepat : f-string

            An f-string formatted against the attributes which
            resolves to the name of the file to be read or written.

        grouppat : f-string

            An f-string formatted against attributes to determine and
            to add a "hdfgroup" parameter to the BOT message prior to
            it being sent into a read or write handler.  The value
            will be used to define an HDF base group path into which
            messages from one stream will be mapped.  This path must
            be unique to the flow client.

        attr : dictionary
        
            An extra dictionary of attributes updated on the
            dictionary of message attributes prior to applying to the
            rule or either patterns.

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
                wactor = ZActor(self.ctx, writer, filename, addrpat)
                waddr = wactor.recv_string()
                if not waddr:
                    err = f"failed to bind any {addrpat} for {filename}"
                    raise RuntimeError(err)
                wactor.addr = waddr # copascetic?
                self.writers["filename"] = wactor

            base_path = maybe["grouppat"].format(**rattr)
            actor = ZActor(self.ctx, write_handler, bot, base_path,
                           self.broker_addr,
                           self.writers[filename].addr)
            return actor
        return
    
