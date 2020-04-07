#!/bin/bash

tstdir=$(dirname $(realpath $BASH_SOURCE))
topdir=$(dirname $tstdir)

jsonnet -m . $tstdir/check-pubsub.jsonnet || exit -1

cat <<EOF > Procfile.many2many
source: $topdir/build/check-pubsub many2many-source.json 
sink: $topdir/build/check-pubsub many2many-sink.json
EOF
cat <<EOF > Procfile.one2one
source: $topdir/build/check-pubsub one2one-source.json 
sink: $topdir/build/check-pubsub one2one-sink.json
EOF
cat <<EOF > Procfile.many2one2many
source: $topdir/build/check-pubsub many2one2many-source.json 
proxy: $topdir/build/check-pubsub many2one2many-proxy.json 
sink: $topdir/build/check-pubsub many2one2many-sink.json
EOF

