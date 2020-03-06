#!/usr/bin/env python3
'''
Main CLI to ZIO 
'''

import logging
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s.%(msecs)03d %(levelname)s\t%(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S')
log = logging.getLogger("zio")

from .mains import  *


def main():
    cli(obj=dict())

if '__main__' == __name__:
    main()
    

