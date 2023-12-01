#!/bin/bash

vals=("256 256 256", "256 256 128", "256 128 256", "128 256 256")
nprocs=(32)
for val in "${nprocs[@]}"
do
 for dims in "${vals[@]}"
  do
 mpirun -n 1 ./ddtest $dims $val 
#  mpirun -n 1 ./ddtest 256 256 128 $val
#  mpirun -n 1 ./ddtest 256 128 256 $val
#  mpirun -n 1 ./ddtest 128 256 256 $val
 done

python3 analysator-test.py ${vals[@]} $val
#  python3 analysator-test.py 256 256 256 $val
#  python3 analysator-test.py 256 256 128 $val
#  python3 analysator-test.py 256 128 256 $val
#  python3 analysator-test.py 128 256 256 $val
done

