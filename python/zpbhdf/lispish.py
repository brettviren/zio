#!/usr/bin/env python3
import pyparsing as pp
import rule                     # bv's modified version

def sexp():
    '''Return an s-expression parser'''
    dstring = pp.QuotedString('"')
    sstring = pp.QuotedString("'")
    variable = pp.Word(pp.alphas +"_", pp.alphanums + '_').setName("variable")
    integer = pp.Word(pp.nums).setParseAction(lambda m:int(m[0]))
    ops  = ">= <= != > < = == + - * / | & "
    ops += "ge le ne gt lt eq add sub mul div or and"
    print (ops)
    operator = pp.oneOf(ops).setName("operator")
    arguments = pp.ZeroOrMore(variable | integer | sstring | dstring).setName("arguments")
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

def test():

    sp = sexp()

    data = dict(a=3,b=2,name="Manfred")
    for toparse in [
            "(== a (+ 1 b))",
            '(== name "Ada")',
            '(== name "Manfred")',
            "(== name 'Manfred')",
            '(!= name "Ada")',
            '(!= name "Manfred")',
            '(| a b)',
            '(| (< a b) (> a b))',
            '(& (= a 3) (= 2 b))',
            '(and (== a 3) (eq 2 b))',
            ]:

        pr = sp.parseString(toparse)
        #if len(pr) == 1 and len(pr[0]) == 1:
        #pr = pr[0]
        print (pr)
        r = rule.Rule(pr)
        print(r)
        result = r.match(data)
        print (f'"{toparse}" on {data} gives {result}')


if '__main__' == __name__:
    test()
    
