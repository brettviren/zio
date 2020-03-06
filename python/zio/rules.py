#!/usr/bin/env python3
'''
Support for rules
'''
from rule import Rule           # bv's hacked version
from . import lispish

import logging
log = logging.getLogger("zio")

def parse(rule_object, **params):
    '''
    Returned a parsed rule object.
    '''
    params.update(rule_object.get('attr',{}))
    log.debug(f'rule attributes: {params}')
    parser = lispish.parser(params)
    scode = rule_object['rule']
    try:
        parsed = parser.parseString(scode)
    except KeyError as e:
        log.error(f'key error "{scode}": "{e}"')
        raise 
    except lispish.ParseException:
        log.error(f'parse error with rule:\n{scode}')
        raise
    return parsed[0]

def evaluate(parsed):
    '''
    Evaluate a parsed rule, return True/False
    '''
    expr = Rule(parsed, return_bool = True)
    return expr.match();

