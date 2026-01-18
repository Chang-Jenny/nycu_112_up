#!/bin/bash

set -x
set -e

make

if [$# -eq 0]; then
    echo "Usage: $0 <ex_[1,2,3,4]>"
    exit 1
fi

option=$1
case $option in
  ex1)
      ./examples/ex1.exp
      ;;
  ex2)
      ./examples/ex2.exp
      ;;
  ex3)
      ./examples/ex3.exp
      ;;
  ex4)
      ./examples/ex4.exp
      ;;
  test)
      ./examples/test.exp
      ;;
  *)
      echo "Usage: $0 <ex_[1,2,3,4]>"
      exit 1
      ;;
esac