#!/bin/sh

tests=`find examples -mindepth 1 -maxdepth 1 -type d`

summary=''
for dir in $tests
do
	testname=`basename $dir`
	echo "$testname parser tests"
	aag=`find $dir -name '*.aag'`

	count=0
	failed=0
	for a in $aag
	do
		n=`basename $a`
		printf "trying %-15s " "$n"
		./boumc < $a > /dev/null 2>/dev/null
		if [ $? -eq 0 ]; then
			echo "[OK]"
		else
			echo "[FAIL]"
			failed=$((failed+1))
		fi	
		count=$((count+1))
	done
	echo
	summary="$summary\n$failed/$count of $testname tests failed"
done

printf "$summary\n\n"
