EXPORT=f:/git/export
.PHONEY: check

CC=gcc
CCWARN=-Wsign-compare -Wall -Wdeclaration-after-statement 
CCDEBUG=-g -O0

all: ttm.exe

clean::
	rm -f ttm.exe ttm ttm.txt test.output tmp

ttm.exe: ttm.c
	${CC} ${CCWARN} ${CCDEBUG} -o ttm ttm.c

ttm.txt::
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement ttm.c > ttm.txt

TESTPROG=-p test.ttm
TESTARGS=a b c
TESTRFLAG=-f test.rs
TESTCMD=./ttm ${TESTPROG} ${TESTRFLAG} ${TESTARGS}
PYCMD=python ttm.py -dT ${TESTPROG} ${TESTRFLAG} ${TESTARGS}

check:: ttm.exe
	rm -f ./test.output
	${TESTCMD} >& ./test.output
	diff -w ./test.baseline ./test.output

pycheck:: ttm.py
	rm -f ./test.output
	${PYCMD} >& ./test.output
	diff -w ./test.baseline ./test.output

git::
	${SH} ./git.sh

install::
	rm -fr ${EXPORT}/ttm/*
	cp -fr ./git/ttm ${EXPORT}
