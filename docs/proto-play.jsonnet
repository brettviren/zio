// j2 -f json proto-play.cpp.j2 <(jsonnet proto-play.jsonnet) |
//    clang-format -style="{BasedOnStyle: google, IndentWidth: 4}"

local podtype = function(c,j) {c:c,j:j};
local string = podtype("std::string", "string");
local int = podtype("int", "number");
local bool = podtype("bool", "boolean");
local void = podtype("void", "null");

local attr = function(name, type, def) { name:name, type:type, def:def };
local message = function(name, attrs=[]) { name:name, attrs:attrs};
local errval = function(key, value, code) {key:key,value:value,code:code};

local accept = function(key, msg, errors=[]) {
    key:key, message:msg, errors:errors
};
local command = function(help, message, type, default, accepts) {
    help:help, message:message, type:type, default:default, accepts:accepts
};


// An event is a message
local event = message;
local sm = function(name, tt=[], type="sm") { type:type, name:name, tt:tt };
local state = function(name) sm(name, type="state");
local trans = function(ini, fin, eve="", grds=[], acts=[], star="") {
    ini:ini, fin:fin, eve:eve, grds:grds,acts:acts,star:star};
// A guard or an action are just names. 

// Some test message/methods
{
    namespace: "foo",
    classname: "Blah",

    events: {
        begin: event("begin", [attr("greeting", string, '""')]),
        done: event("done"),
    },

    smcontext: "Quax",
    machines: [ sm("jigger", [
        trans("start","waiting",$.events.begin,
              ["is_nice", "is_capital"],
              ["print_event", "print_event"], "*"),
        trans("start","done",$.events.begin,
              ["is_mean", "is_capital"], ["print_event"]),
        trans("start","done",$.events.done, acts=["print_event"])])],

    eve_grds: [std.split(s,":") for s in  std.set(["%s:%s"%[t.eve.name,g] for m in $.machines for t in m.tt for g in t.grds])],
    eve_acts: [std.split(s,":") for s in  std.set(["%s:%s"%[t.eve.name,a] for m in $.machines for t in m.tt for a in t.acts])],


    conn_msg: message("connect", [ attr("endpoint",string, '""') ]),
    bind_msg: message("bind", [ attr("port",int, "-1") ]),
    conn_status: message("status", [ attr("ok", bool, "false") ]),
    bind_status: message("status", [ attr("port", int, "-1") ]),

    methods: [
        command("Connect protocol socket to the given endpoint",
                $.conn_msg,  bool, "false",
                [ accept("ok", $.conn_status, [
                    errval("error", "true",
                           'raise std::runtime_error("connect failed");')])]),
        command("Bind protocol socket to the given port number, return it",
                $.bind_msg, int, "-1",
                [ accept("port", $.bind_status, [
                    errval("error", "true",
                           'raise std::runtime_error("bind failed");')])]),


    ]
}
