#!/usr/bin/env python3
import json
from .. import jsonnet
import click
from .. import rules

from zio.util import modlog
log = modlog(__name__)

@click.group("flow")
@click.pass_context
def cli(ctx):
    '''
    ZIO FLOW command line interface
    '''


def typeit(v):
    try:
        return int(v)
    except ValueError:
        pass
    try:
        return float(v)
    except ValueError:
        pass
    return v                    # fixme: should recurs...

def attrify(attrs):
    '''
    Return dictionary of attribute values parsed from list of "k=v" strings.
    '''
    ret = dict()
    for attr in attrs:
        try:
            k,v = attr.split('=', 1)
        except ValueError:
            log.error(f'failed to parse {attr}')
            continue
        ret[k] = typeit(v)
    return ret

@cli.command("test-ruleset")
@click.option("-r","--ruleset",type=click.Path(), required=True,
              help="A file in JSON or Jsonnet format providing the ruleset")
@click.option("-v","--verbosity", default="info",
              help="Set logging level (debug, info, warning, error, critical)")
@click.argument("attrs", nargs=-1)
def test_ruleset(ruleset, verbosity, attrs):
    '''
    Test a rule set by giving attributes as key=value on command line.
    '''
    log.level = getattr(logging, verbosity.upper(), "INFO")

    msg_attr = attrify(attrs)

    rs = jsonnet.load(ruleset)
    # fixme: move this to a module
    for ind,robj in enumerate(rs):
        attr = dict(robj.get('attr',{}), **msg_attr)
        log.debug(f'attr: {attr}')

        def dump(errtype, err):
            log.error(f'{errtype} "{err}"\n{rule}\n{attr}')

        # do parsing
        try:
            parsed = rules.parse(robj, **attr)
        except KeyError as ke:
            dump("key error", ke)
            continue

        # do evaluating
        # can call rules.evaluate() but we want to print extra stuff here
        log.debug(f'parsed expression: {parsed}')
        expr = rules.Rule(parsed) #, return_bool = True)
        log.debug(f'rule expression: {expr}')
        tf = expr.match();
        log.debug(f'rule evaluation: {tf}')

        # do string interpolation no the "pat" patterns
        filepat = robj['filepat']
        try:
            path = filepat.format(**attr)
        except KeyError as ke:
            dump(f'key error "{filepat}"', ke)
            continue

        grouppat = robj['grouppat']
        try:
            group = grouppat.format(**attr)
        except KeyError as ke:
            dump(f'key error "{grouppat}"', ke)
            continue

        rw = robj['rw']
        tf = "TRUE" if tf else "FALSE"
        log.info(f'#{ind} {tf:5s} {rw} {path}:/{group}')
        

@cli.command("flow-file-server")
@click.option("-b","--bind", default="tcp://127.0.0.1:5555",
              help="An address to which the server shall bind")
@click.option("-f","--format", default="hdf", type=click.Choice(["hdf"]),
              help="File format")
@click.option("-n","--name", default="file-server",
              help="The Zyre node name for the server")
@click.option("-p","--port", default="flow",
              help="The port name for the server socket")
@click.option("-v","--verbosity", default="info",
              help="Set logging level (debug, info, warning, error, critical)")
@click.argument("ruleset")
def file_server(bind, format, name, port, verbosity, ruleset):
    '''Source and sink ZIO flow protocol to file

    This brings back-end reader and writer handlers to external
    clients via the flow broker.  A ruleset factory dynamically spawns
    handlers based on incoming BOT flow messages.  The file format
    maps to different flavors of handlers.

    '''
    import zmq
    from zio import Port, Message, Node
    from zio.flow.broker import Broker
    from zio.flow.factories import Ruleset as Factory
    from zio.jsonnet import load as jsonnet_load

    # For now we only support HDF.  In future this may be replaced by
    # a mapping from supported format to module providing handlers
    from zio.flow.hdf import writer, reader
    assert(format == "hdf")

    #log.level = getattr(logging, verbosity.upper(), "INFO")

    ruleset = jsonnet_load(ruleset)

    # fixme: is it really needed to create a common ctx?
    ctx = zmq.Context()
    factory = Factory(ctx, ruleset, 
                      wactors=((writer.file_handler,
                                ("inproc://"+format+"writer{port}")),
                               (writer.client_handler,
                                (bind,))),
                      ractor=(reader.handler,(bind,)))

    node = Node(name)
    sport = node.port(port, zmq.SERVER)
    sport.bind(bind)
    node.online()    
    log.info(f'flow-broker {name}:{port} online at {bind}')

    # this may throw
    broker = Broker(sport, factory)

    log.info(f'flow-broker {name} entering loop')
    while True:
        try:
            broker.poll(10000)
        except TimeoutError:
            node.peer.drain()
            log.debug(f'flow-broker {name} is lonely')
            log.debug(node.peer.peers)
        except Exception as e:
            log.error(e)
            continue

    broker.stop()

@cli.command("flow-send-tens")
@click.option("-n","--number",default=10,
              help="Number of TENS messages to generate")
@click.option("-c","--connect", default="tcp://127.0.0.1:5555",
              help="An address to which this client shall connect")
@click.option("-s","--shape", default="6000,800",
              help="Comma separated list of integers giving tensor shape")
@click.option("-v","--verbosity", default="info",
              help="Set logging level (debug, info, warning, error, critical)")
@click.argument("attrs", nargs=-1)
def send_tens(number, connect, shape, verbosity, attrs):
    '''
    Generate and flow some TENS messages.
    '''
    import zmq
    from zio import Port, Message, Node
    from zio.flow import Flow

    log.level = getattr(logging, verbosity.upper(), "INFO")

    msg_attr = attrify(attrs)

    cnode = Node("flow-send-tens")
    cport = cnode.port("output", zmq.CLIENT)
    cport.connect(connect)
    cnode.online()
    cflow = Flow(cport)
    
    shape = list(map(int, shape.split(',')))
    size = 1
    for s in shape:
        size *= s

    attr = dict(credit=2, direction="extract", **msg_attr)
    bot = Message(label=json.dumps(attr))
    cflow.send_bot(bot)
    bot = cflow.recv_bot(5000);
    log.debug('flow-send-tens: BOT handshake done')
    assert(bot)

    tens_attr = dict(shape=shape, word=1, dtype='u') # unsigned char
    attr["TENS"] = dict(tensors=[tens_attr], metadata=dict(source="gen-tens"))
    label = json.dumps(attr)
    payload = [b'X'*size]

    for count in range(number):
        msg = Message(label=label,payload=payload)
        cflow.put(msg)
        log.debug(f'flow-send-tens: {count}: {msg}')
        
    log.debug(f'flow-send-tens: send EOT')
    cflow.send_eot(Message())
    log.debug(f'flow-send-tens: recv EOT (waiting)')
    cflow.recv_eot()
    log.debug(f'flow-send-tens: going offline')
    cnode.offline()
    log.debug(f'flow-send-tens: end')
    
@cli.command("flow-recv-tens")
@click.option("-n","--number",default=10,
              help="Number of TENS messages to recv before exiting, 0=forever")
@click.option("-c","--connect", default="tcp://127.0.0.1:5555",
              help="An address to which this client shall connect")
@click.option("-v","--verbosity", default="info",
              help="Set logging level (debug, info, warning, error, critical)")
@click.argument("attrs", nargs=-1)
def recv_tens(number, connect, verbosity, attrs):
    '''
    Client to recv flow of TENS messages.
    '''
    import zmq
    from zio import Port, Message, Node
    from zio.flow import Flow

    log.level = getattr(logging, verbosity.upper(), "INFO")

    msg_attr = attrify(attrs)

    cnode = Node("flow-recv-tens")
    cport = cnode.port("input", zmq.CLIENT)
    cport.connect(connect)
    cnode.online()
    cflow = Flow(cport)
    
    attr = dict(credit=2, direction="inject", **msg_attr)
    bot = Message(label=json.dumps(attr))
    cflow.send_bot(bot)
    bot = cflow.recv_bot(5000);
    log.debug('flow-recv-tens: BOT handshake done')
    assert(bot)

    count = 0
    while True:
        if number > 0 and count == number:
            break
        ++count
        msg = cflow.get()
        log.info(f'flow-recv-tens: {count}: {msg}')
        if msg is None:
            cflow.send_eot()
            cnode.offline()
            log.debug('flow-recv-tens: EOT whille receiving')
            return
        
    log.debug(f'flow-recv-tens: send EOT')
    cflow.send_eot(Message())
    log.debug(f'flow-recv-tens: recv EOT (waiting)')
    cflow.recv_eot()
    log.debug(f'flow-recv-tens: going offline')
    cnode.offline()
    log.debug(f'flow-recv-tens: end')
    



@cli.command("flow-recv-server")
@click.option("-n","--number",default=10,
              help="Number of TENS messages to generate")
@click.option("-b","--bind", default="tcp://127.0.0.1:5555",
              help="An address to which this client shall bind")
@click.option("-s","--shape", default="6000,800",
              help="Comma separated list of integers giving tensor shape")
@click.option("-v","--verbosity", default="info",
              help="Set logging level (debug, info, warning, error, critical)")
@click.argument("attrs", nargs=-1)
def recv_server(number, bind, shape, verbosity, attrs):
    '''
    A simple server to catch some TENS messages from flow and dump them.
    '''
    import zmq
    from zio import Port, Message, Node
    from zio.flow import Flow

    log.level = getattr(logging, verbosity.upper(), "INFO")

    msg_attr = attrify(attrs)

    snode = Node("server")
    sport = snode.port("sport", zmq.SERVER)
    sport.bind(bind)
    snode.online()
    sflow = Flow(sport)

    bot = sflow.recv_bot();
    assert (bot)
    lobj = bot.label_object
    lobj["direction"] = "inject"
    bot.label_object = lobj
    sflow.send_bot(bot)
    log.debug('flow-recv-server: BOT handshake done')
    sflow.flush_pay()
    log.debug('flow-recv-server: looping')
    while True:
        msg = sflow.get(1000)
        log.info(f'flow-recv-server: {msg}')
        if not msg or 'EOT' == msg.label_object['flow']:
            log.debug('flow-recv-server: got EOT')
            sflow.send_eot()     # answer
            break
        
    snode.offline()
    log.debug(f'flow-recv-server: end')    
