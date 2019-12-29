#!/bin/bash
for mo in *.org ; do
    grep -q setupfile $mo || continue
    ho=$(basename $mo .org).html
    # poor man's make
    if [ ! -f "$ho" -o  "$mo" -nt "$ho" ] ; then
        emacs $mo --batch -f org-html-export-to-html --kill 2>/dev/null || echo "$mo failed"
    fi
done

