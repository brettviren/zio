// Generate some test input files for check-pubsub.

// Set rate of msg gen in Hz.  Note: if PUB/SUB used, N sources in one
// app reports Nx rate, if N sinks in one app, another factor of N
// should be reported, if downstream SUBs can't keep up, its rate will
// reflect lost messages.  Note: if PUSH/PULL is used, no messages
// will be lost and back-pressure will cause upstream to wait and if
// 1->N pattern is used then downstream reflect round-robin (and not
// fan-out like PUB/SUB).
local rate = 500;             
local msize = 4096; // 65536;
local nchirp = rate*100;
local give_sock = "PUB";       // set the "PUB" socket type, can be "PUSH"
local take_sock = "SUB";       // set the "SUB" socket type, can be "PULL"

// A TCP bind address, change to public IP for actual network traffic.
// Port number will be selected dynamically.  Connect will use
// discovery.
local bind_addr = "127.0.0.1";


local zmq = { "PAIR":0, "PUB":1, "SUB":2, "REQ":3, "REP":4,
              "DEALER":5, "ROUTER":6, "PULL":7, "PUSH":8 };

// utility functions to enforce schema
local cps = {
    app :: function(name, type, ports=[]) {
        name:name, type:type, ports:ports,
    },

    port :: function(name, stype, endpoints=[]) {
        name:name, stype:zmq[stype], endpoints:endpoints
    },

    // make endpoints
    bind :: function(address) {
        link:"bind", address: address,
    },
    connect :: function(address) {
        link:"connect", address: address,
    },
};
    
// extract some values used in many2one2many
local cfg = {
    source: {
        name: "check-pubsub-source",
        pubs: ["src-pub%d"%n for n in std.range(0,9)],
    },
    proxy: {
        name: "check-pubsub-proxy",
        subs: ["prx-sub0"],
        pubs: ["prx-pub0"],
    },
    sink: {
        name: "check-pubsub-sink",
        subs: ["snk-sub%d"%n for n in std.range(0,9)],
    },
};


local proc = {
    source: {
        size: msize,                // bytes
        rate: rate,
        nchirp: nchirp,             // how many message before emitting a log
    },
    proxy: {
        nchirp: nchirp,
    },
    sink: {
        nchirp: nchirp,
    },
};

{
    "one2one-source.json": cps.app("check-pubsub-source", "source",
                                   [cps.port("give", give_sock, [cps.bind([bind_addr,0])])]) + proc,
    "one2one-sink.json": cps.app("check-pubsub-sink", "sink",
                                 [cps.port("take", take_sock,
                                           [cps.connect(["check-pubsub-source", "give"])])]) + proc,


    "many2one2many-source.json": cps.app(cfg.source.name, "source",
                                         [cps.port(name, give_sock,
                                                   [cps.connect([cfg.proxy.name, cfg.proxy.subs[0]])])
                                          for name in cfg.source.pubs]) + proc,
    "many2one2many-proxy.json": cps.app(cfg.proxy.name, "proxy",
                                        [cps.port(name, take_sock, [cps.bind([bind_addr,0])])
                                         for name in cfg.proxy.subs] +
                                        [cps.port(name, give_sock, [cps.bind([bind_addr,0])])
                                         for name in cfg.proxy.pubs]) + proc,
    "many2one2many-sink.json": cps.app(cfg.sink.name, "sink",
                                       [cps.port(name, take_sock,
                                                 [cps.connect([cfg.proxy.name, cfg.proxy.pubs[0]])])
                                        for name in cfg.sink.subs]) + proc,

    "many2many-source.json": cps.app(cfg.source.name, "source",
                                     [cps.port(pname, give_sock,
                                               [cps.bind([bind_addr,0])])
                                      for pname in cfg.source.pubs]) + proc,
    "many2many-sink.json": cps.app(cfg.sink.name, "sink",
                                   [cps.port(sname, take_sock,
                                             [cps.connect([cfg.source.name, pname])
                                              for pname in cfg.source.pubs])
                                    for sname in cfg.sink.subs]) + proc,
}
