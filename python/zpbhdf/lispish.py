#!/usr/bin/env python3
import pyparsing as pp
import rule                     # bv's modified version

def sexp(params=None):
    '''Return an s-expression parser'''
    params = params or dict()
    dstring = pp.QuotedString('"').setName("stringliteral")
    sstring = pp.QuotedString("'").setName("stringliteral")
    param = pp.Word(pp.alphas +"_", pp.alphanums + '_').setParseAction(lambda m:params.get(m[0],m[0]))
    integer = pp.Word(pp.nums).setParseAction(lambda m:int(m[0]))
    ops  = ">= <= != > < = == + - * / | & "
    ops += "ge le ne gt lt eq add sub mul div or and"
    #print (ops)
    operator = pp.oneOf(ops).setName("operator")
    arguments = pp.ZeroOrMore(param | integer | sstring | dstring).setName("arguments")
    content = operator + arguments


    # expr = pp.OneOrMore(pp.nestedExpr("(",")",
    #                                   content=content))

    lp = pp.Suppress("(")
    rp = pp.Suppress(")")
    ret = pp.Forward()
    #sexp_list = pp.Forward()
    sexp_list = pp.Group(lp + content + rp)
    #sexp_list << (lp + content + rp)
    ret << (lp + pp.ZeroOrMore(content | sexp_list) + rp)
    #ret = pp.Group(lp + pp.ZeroOrMore(content | sexp_list) + rp)
    return ret

def dump_parsed(pr):
    for one in pr:
        print (one, type(one), dir(one))

def test():

    params = dict(a=3,b=2,name="Manfred")
    sp = sexp(params)


    for want, toparse in [
            (True, "(== a (+ 1 b))"),
            (False, '(== name "Ada")'),
            (True, '(== name "Manfred")'),
            (True, "(== name 'Manfred')"),
            (True, '(!= name "Ada")'),
            (False, '(!= name "Manfred")'),
            (3, '(| a b)'),
            (True, '(| (< a b) (> a b))'),
            (True, '(& (= a 3) (= 2 b))'),
            (True, '(and (== a 3) (eq 2 b))'),
            (True, '(!= name "name")'),
            ]:

        pr = sp.parseString(toparse)
        #if len(pr) == 1 and len(pr[0]) == 1:
        #pr = pr[0]
        print ("parsed:",pr)
        r = rule.Rule(pr)
        print("rule:",r)
        result = r.match()
        print (f'"{toparse}" on {params} gives {result}')
        assert(want == result)


if '__main__' == __name__:
    test()
    
