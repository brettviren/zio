plantuml_src = $(wildcard *.plantuml)
plantuml_svg = $(patsubst %.plantuml,%.svg,$(plantuml_src))
plantuml_png = $(patsubst %.plantuml,%.png,$(plantuml_src))

main_org = $(shell grep setup.org *.org | sed -e 's/\(.*\.org\):.*/\1/')
main_html = $(patsubst %.org,%.html,$(main_org))

define org2html =
# emacs --no-site-file -Q --batch $< --eval="(progn (load-file \"htmlize.el\") (require 'org) (org-html-export-to-html))" --kill
emacs --no-site-file -Q --batch $< --eval="(progn (load-file \"export.el\") (org-html-export-to-html))" --kill
endef

all: all_png all_svg all_html
all_svg: $(plantuml_svg)
all_png: $(plantuml_png)
all_html: $(main_html) index.html


%.svg : %.plantuml
	plantuml -tsvg $<
%.png : %.plantuml
	plantuml -tpng $<

index.html: index.org nav.org

%.html : %.org setup.org nav.org
	$(org2html)

nuke:
	rm -f $(plantuml_svg) $(main_html)
