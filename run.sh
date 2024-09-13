#!/bin/bash

exec_dir=./build

# MQBucket
echo "Results" > results_rbp_1_MQBucket.log
for j in {1..10}
do
	${exec_dir}/rbp bucket ising 1000 1 4 128 128 7 64 1 1 >> results_rbp_1_MQBucket.log
done

echo "Results" > results_rbp_48_MQBucket.log
for j in {1..10}
do
	${exec_dir}/rbp bucket ising 1000 48 192 128 128 7 64 1 1 >> results_rbp_48_MQBucket.log
done

# MQ
echo "Results" > results_rbp_1_MQ.log
for j in {1..10}
do
	${exec_dir}/rbp heap ising 1000 1 4 128 128 7 64 1 1 >> results_rbp_1_MQ.log
done

echo "Results" > results_rbp_48_MQ.log
for j in {1..10}
do
	${exec_dir}/rbp heap ising 1000 48 192 128 128 7 64 1 1 >> results_rbp_48_MQ.log
done


# MQ Plain
echo "Results" > results_rbp_1_MQPlain.log
for j in {1..10}
do
	${exec_dir}/rbp heap ising 1000 1 4 1 1 7 64 0 1 >> results_rbp_1_MQPlain.log
done

echo "Results" > results_rbp_48_MQPlain.log
for j in {1..10}
do
	${exec_dir}/rbp heap ising 1000 48 192 1 1 7 64 0 1 >> results_rbp_48_MQPlain.log
done




