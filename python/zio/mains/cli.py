#!/usr/bin/env python3
import click
@click.group("zio")
@click.pass_context
def cli(ctx):
    '''
    ZIO command line interface
    '''
