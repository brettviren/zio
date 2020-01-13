#!/usr/bin/env python3
import pyparsing as pp
import rule


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
sexp = pp.Forward()
#sexp_list = pp.Forward()
sexp_list = pp.Group(lp + content + rp)
#sexp_list << (lp + content + rp)
sexp << (lp + pp.ZeroOrMore(content | sexp_list) + rp)
#sexp = pp.Group(lp + pp.ZeroOrMore(content | sexp_list) + rp)
expr = sexp


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

    pr = expr.parseString(toparse)
    #if len(pr) == 1 and len(pr[0]) == 1:
    #pr = pr[0]
    print (pr)
    r = rule.Rule(pr)
    print(r)
    result = r.match(data)
    print (f'"{toparse}" on {data} gives {result}')
