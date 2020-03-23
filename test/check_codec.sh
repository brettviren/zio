#!/bin/bash


pcmd="zio"
if [ -z "$(which $pcmd)" ] ; then
    echo "test_codec.sh must be run inside environment with $pcmd available"
    exit
fi
pcmd="zio check-codec"
ccmd="./build/check_codec"
if [ ! -x $ccmd ] ; then
    echo "test_codec.sh must be run inside environment with $ccmd available"
    exit
fi

set -e
set -x

for server in "$pcmd" "$ccmd"
do
    for client in "$pcmd" "$ccmd"
    do

        pids=""
        $server server &
        pids="$(jobs -p %%)"
        $client client &
        pids="$(jobs -p %%) $pids"
        wait $pids

        pids=""
        $server router &
        pids="$(jobs -p %%)"
        $client dealer &
        pids="$(jobs -p %%) $pids"
        wait $pids
        
    done
done
