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

check:: ttm.exe
	rm -f ./test.output
	${TESTCMD} >& ./test.output
	diff -w ./test.baseline ./test.output
