PYTHON = python
NAME_DIST = /home/rushing/python/name_dist.py

all:	libavl.a avl_test

CFLAGS = -g -O2 -Wall

avl.o:	avl.c avl.h

avl_test:	avl.o	test.o
	$(CC) avl.o test.o	-o avl_test

libavl.a:	avl.o
	ar scq libavl.a avl.o

clean:
	rm -f *.o *.obj *.lib *.exe *.pdb *.a *.pyc *~ avl_test

dist:
	tar -zcvf /tmp/avl.tar.gz `find . -type f -and -not -regex "\\./.*\(\(RCS\)\|\(OLD\)\|~\|\(.pyc\)\).*"  -print| sort `
	$(PYTHON) $(NAME_DIST) avl
