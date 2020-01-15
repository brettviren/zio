#!/usr/bin/env python3
import pyparsing as pp
import rule

# S-expression grammar
w = pp.Word(pp.alphas +"_", pp.alphanums + '_')
operator = pp.oneOf(">= <= != > < = +").setName("operator")

lp = pp.Suppress("(")
rp = pp.Suppress(")")
sexp = pp.Forward()
sexp_list = pp.Forward()
sexp_list = pp.Group(lp + pp.ZeroOrMore(sexp) + rp)
sexp << (operator | w | sexp_list)
#sexp_list << (sexp | (sexp + sexp_list))
#sexp << (w | (lp + sexp_list + rp))

s = "(> a (+ 1 b))"

try:
    pr = sexp.parseString(s)
    print(pr[0])

except pp.ParseException as e:
    print(e)
    raise

r=pr[0]

#r = ['>', 'a', ['+', 1, 'b']]

data = dict(a=1,b=2)
result = rule.Rule(r).match(data)
print (f'"{s}" on {data} gives {result}')
  
