#!/usr/bin/env python3
'''
Main CLI to ZIO 
'''
import click

from zio.util import mainlog
mainlog()

from .mains import  peers, flow, domo, check
cli = click.CommandCollection(sources=[peers.cli, flow.cli, domo.cli, check.cli])

def main():
    cli(obj=dict())

if '__main__' == __name__:
    main()
    

