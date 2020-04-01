// Generate some test input files for check-pubsub.

local zmq = { "PUB":1, "SUB":2 };

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

local rate = 1000000;              // hz
local nchirp = rate/10;

local proc = {
    source: {
        size: 65536,                // bytes
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
                                   [cps.port("give", "PUB", [cps.bind("")])]) + proc,
    "one2one-sink.json": cps.app("check-pubsub-sink", "sink",
                                 [cps.port("take", "SUB",
                                           [cps.connect(["check-pubsub-source", "give"])])]) + proc,

    "many2one2many-source.json": cps.app(cfg.source.name, "source",
                                         [cps.port(name, "PUB",
                                                   [cps.connect([cfg.proxy.name, cfg.proxy.subs[0]])])
                                          for name in cfg.source.pubs]) + proc,
    "many2one2many-proxy.json": cps.app(cfg.proxy.name, "proxy",
                                        [cps.port(name, "SUB", [cps.bind("")])
                                         for name in cfg.proxy.subs] +
                                        [cps.port(name, "PUB", [cps.bind("")])
                                         for name in cfg.proxy.pubs]) + proc,
    "many2one2many-sink.json": cps.app(cfg.sink.name, "sink",
                                       [cps.port(name, "SUB",
                                                 [cps.connect([cfg.proxy.name, cfg.proxy.pubs[0]])])
                                        for name in cfg.sink.subs]) + proc,
}
