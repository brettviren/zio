#!/bin/bash
zio test-ruleset -r tests/example-ruleset.jsonnet \
    direction=inject \
    type=depo \
    jobname=myjob \
    stream=proton

zio test-ruleset -r tests/example-ruleset.jsonnet \
    direction=extract \
    type=frame \
    jobname=myjob \
    stream=proton
