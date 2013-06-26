# -*- Mode: Python; coding: iso-8859-1 -*-
"""
distutil setup script
"""

from distutils.core import setup, Extension

# set file generation umask, so that group gets write permissions on
# generated files
import os
os.umask(2)

src     = ['AVLmodule.c']

ext_macros = []
#ext_macros = ['DEBUG_AVL']

setup (name         = "avl",
       version      = "2.1.3",
       description  = "AVL Tree Objects for Python",
       author       = 'Samual M Rushing',
       author_email = "[hidden]",
       maintainer   = "Berthold Höllmann",
       maintainer_email = "hoel@gl-group.com",
       license      = "BSD",
       url          = 'http://www.nightmare.com/squirl/python-ext/avl/',
       libraries    = [("avl", {"sources": ["avl.c"]})],
       ext_modules  = [Extension('avl', src,
                                 define_macros=ext_macros)])

## ======================================================================
##   Update Informations
##  $Log: setup.py,v $
##  Revision 1.4  2005/09/20 19:27:27  rushing
##  version
##
##  Revision 1.3  2005/08/19 22:28:54  rushing
##  update version number
##
##  Revision 1.2  2005/06/02 01:17:12  rushing
##  minor change to url
##
##  Revision 1.1  2005/06/02 01:16:40  rushing
##  Initial revision
##
##  Revision 1.5  2005/05/31 07:53:26  hoel
##  prepared release 2.1.0
##
##  Revision 1.4  2005/05/31 07:08:23  hoel
##  * fixes for Linux_x86_64
##
##  * Bumped version number to 2.1.0
##
##  Revision 1.3  2002/10/15 14:15:53  hoel
##  bumped version number
##
##  Revision 1.2  2002/06/12 14:18:41  hoel
##  *** empty log message ***
##

# Local Variables:;
# compile-command:"python setup.py build";
# End:;
