#!/usr/bin/env python3

import zio
import sys
import time
import click
from .cli import cli

@cli.command("peers")
@click.option("-t","--timeout", default=1000,
              help='Time to wait for peers in millisecond, 0 waits forever')
@click.option("-w","--wait-for", type=str, multiple=True,
              help='Wait for one or more peers before exiting, uses timeout')
@click.option("--pre-sleep", type=int, default=0,
              help='Number of seconds to sleep after our peer goes on line and before looking for friends')
@click.option("--post-sleep", type=int, default=0,
              help='Number of seconds to sleep after our peer goes on line and before exiting')
@click.option("-n","--name", default="lurker",
              help='Set a Zyre nick name for this node')
@click.argument("headers",nargs=-1)
def peers(timeout, wait_for, pre_sleep, post_sleep, name, headers):
    '''
    Join Zyre and discover peers.
    '''
    hh = dict()
    for header in headers:
        k,v = header.split('=')
        hh[k] = v

    me = zio.Peer(name, **hh)

    time.sleep(pre_sleep)

    got = me.party(wait_for, timeout);
    for pi in got.values():
        click.echo(f"{pi.uuid} {pi.nick}")
        for k,v in pi.headers.items():
            click.echo(f"\t{k} = {v}")

    time.sleep(post_sleep)

    return
    

