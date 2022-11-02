#!/bin/bash

cwd=$(pwd)
case ${cwd} in
  */utils)
    cd ../..
    ;;
  */src)
    cd ..
    ;;
esac

grc=0
LOG=test.log
> $LOG

echo "== check" >> $LOG
./bin/bdj4 --check_all >> $LOG 2>&1
rc=$?
if [[ $rc -ne 0 ]]; then
  echo "check: FAIL"
  grc=1
else
  echo "check: OK"
fi

if [[ $grc -eq 0 ]]; then
  echo "== dbtest" >> $LOG
  ./src/utils/mktestsetup.sh --force >> $LOG 2>&1
  ./src/utils/dbtest.sh >> $LOG 2>&1
  rc=$?
  if [[ $rc -ne 0 ]]; then
    echo "dbtest: FAIL"
    grc=1
  else
    echo "dbtest: OK"
  fi
else
  echo "dbtest not run"
fi

if [[ $grc -eq 0 ]]; then
  echo "== testsuite" >> $LOG
  ./src/utils/mktestsetup.sh --force >> $LOG 2>&1
  ./bin/bdj4 --testsuite
  rc=$?
  if [[ $rc -ne 0 ]]; then
    echo "testsuite: FAIL"
    grc=1
  else
    echo "testsuite: OK"
  fi
else
  echo "testsuite not run"
fi

exit $grc