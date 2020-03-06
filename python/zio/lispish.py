#!/usr/bin/env python3

import rule                     # bv's modified version
from pyparsing import *
def parser(params = None):

    params = params or dict()

    TRUE = CaselessKeyword("true").setParseAction(lambda m: True)
    FALSE = CaselessKeyword("false").setParseAction(lambda m: False)
    boolean = TRUE | FALSE

    param = Word(alphas +"_", alphanums + '_').setParseAction(lambda m:params[m[0]])
    sstring = QuotedString('"').setName("stringliteral")
    dstring = QuotedString("'").setName("stringliteral")
    string = sstring | dstring
    integer = Word(nums).setParseAction(lambda m:int(m[0]))

    ops  = ">= <= != > < = == + - * / | & "
    ops += "ge le ne gt lt eq add sub mul div or and"
    operator = oneOf(ops).setName("operator")
    atom = boolean | string | integer | operator | param

    lp = Suppress("(")
    rp = Suppress(")")
    sexp = Forward()
    func = Group( lp + ZeroOrMore(sexp) + rp )
    sexp << ( atom | func )
    return sexp


def dump_parsed(pr):
    for one in pr:
        print (one, type(one), dir(one))

def test():

    params = dict(a=3,b=2,name="Manfred")
    sp = parser(params)


    try:
        pr = sp.parseString("undefined")
    except KeyError:
        pass
    else:
        raise RuntimeError("should have gotten an KeyError on 'undefined'")


    for want, toparse in [
            (True, "(or 0 1)"),
            (True, "true"),
            (True, "True"),
            (False, "false"),
            (True, "(or (= a 5) (= b 2))"),
            (True, "(or (= a 5) (= b 2))"),
            (True, "(and (eq name 'Manfred') (eq b 2))"),
            (True, "(and (= name 'Manfred') (or (= a 5) (= b 2)))"),
            (True, "(== a (+ 1 b))"),
            (False, '(== name "Ada")'),
            (True, '(== name "Manfred")'),
            (True, "(== name 'Manfred')"),
            (True, '(!= name "Ada")'),
            (False, '(!= name "Manfred")'),
            (True, '(| a b)'),
            (True, '(| (< a b) (> a b))'),
            (True, '(& (= a 3) (= 2 b))'),
            (True, '(and (== a 3) (eq 2 b))'),
            (True, '(!= name "name")'),
            ]:

        pr = sp.parseString(toparse, parseAll=True)
        #if len(pr) == 1 and len(pr[0]) == 1:
        print (pr)
        pr = pr[0]
        print ("parsed:",type(pr), pr)
        if isinstance(pr, ParseResults):
            r = rule.Rule(pr, return_bool=True)
            print("rule:",r)
            result = r.match()
            print (f'Rule result: "{toparse}" on {params} gives {result}')
        else:
            result = True if pr else False
            print (f'Literal result: "{toparse}" on {params} gives {result}')
            
        assert(want == result)
        print ()


if '__main__' == __name__:
    test()
    
