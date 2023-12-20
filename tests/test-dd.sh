#!/bin/bash

vals=("256 256 256", "256 256 128", "256 128 256", "128 256 256")
# vals=("1024 736 736")
nprocs=(32)
# nprocs=(2400)
for val in "${nprocs[@]}"
do
 for dims in "${vals[@]}"
  do
 mpirun -n 1 ./ddtest $dims $val 
#  mpirun -n 1 ./ddtest 256 256 128 $val
#  mpirun -n 1 ./ddtest 256 128 256 $val
#  mpirun -n 1 ./ddtest 128 256 256 $val
 done

done

