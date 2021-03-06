#!/bin/bash

DESC="Compile and run the code, with input if exists"
. lib/common.sh

if [ ! -e "$SRC" ]; then
  echo "Source file does not exist; refusing to run."
  exit 1
fi

g++ --debug "$SRC" -o "$EXE" -DDEBUG -Wall -Wextra $CXXFLAGS || exit $?
if [ "$2" = "-v" ]; then EXE="valgrind --leak-check=full --show-leak-kinds=all $EXE"; fi

if [ "$2" = "-p" ]; then
  if [ ! -e "$INFILE" ]; then
    echo "$INFILE needed to run profiling."
    exit 1
  fi
  "$EXE" < "$INFILE" > /dev/null &
  PID=$!
  perf record -F 99 -p $PID -g -- sleep 30 2>/dev/null # || echo "The program exited before perf is ready to record; try with a bigger input." && exit 1
  kill $PID
  flamegraph.pl <(stackcollapse-perf.pl <(perf script)) > perf.svg || echo "Flame graph generation failed." && exit 1
  rm -f perf.data
  $BROWSER perf.svg
  exit 0
fi

if [ -e "$INFILE" ]; then time $EXE < "$INFILE"; else time $EXE; fi
