(load-file "htmlize.el") 
(require 'org)
;;(require 'ob-python)
(setq org-confirm-babel-evaluate nil)  ;trusting!
(org-babel-do-load-languages
 'org-babel-load-languages
 '(
;;   (python . t)
;;   (ipython . t)
   (shell . t)
   (ditaa . t)
   (dot . t)
   (C . t)
   (haskell . t)
   (sqlite . t)
   (plantuml . t)
   ))
