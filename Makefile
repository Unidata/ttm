.PHONEY: check

all: ttm.exe

clean::
	rm -f ttm.exe ttm ttm.txt test.output tmp

ttm.exe: ttm.c
	gcc -Wsign-compare -Wall -Wdeclaration-after-statement -g -O0 -o ttm ttm.c

ttm.txt::
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement ttm.c > ttm.txt

TESTPROG=-f test.ttm
TESTARGS=a b c
TESTRFLAG=-r test.rs

check:: ttm.exe
	rm -f ./test.output
	./ttm ${TESTRFLAG} ${TESTPROG} ${TESTARGS} >& ./test.output
	diff -w ./test.baseline ./test.output

