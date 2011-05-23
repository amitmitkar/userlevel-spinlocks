#!/bin/bash

spin=1
spinner()
{
    spin_mod=$(($spin % 4))
    case $spin_mod in
        0)
            echo -ne "\b/"
            ;;
        1)
            echo -ne "\b-"
            ;;
        2)
            echo -ne "\b\\"
            ;;
        3)
            echo -ne "\b|"
            ;;
    esac
    spin=$(($spin + 1))
}

niters=$1
nthreads=$2

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`
rm -f numbers.t*
echo "Run $niters test runs for t-tas and tas with $nthreads threads"
echo -n "Running ttas tests ..."
for i in `seq 1 $niters`
do
    op=`./test_lock ttas $nthreads`
    echo $op | sed -e 's/.*AVERAGE USAGE //g' >> numbers.ttas
    spinner
done
echo -en "\bdone"
bc_str=`cat numbers.ttas  | sed -e  's/$/ + /g'`
bc_str=`echo $bc_str  0`
ttas_avg=`echo "scale=5;($bc_str)/$niters" | bc` 
echo " avg = $ttas_avg"

echo -n "Running tas tests ..."
for i in `seq 1 $niters`
do
    op=`./test_lock tas $nthreads`
    echo $op | sed -e 's/.*AVERAGE USAGE //g' >> numbers.tas
    spinner
done
echo -en "\bdone"
bc_str=`cat numbers.tas  | sed -e  's/$/ + /g'`
bc_str=`echo $bc_str  0`
tas_avg=`echo "scale=5;($bc_str)/$niters" | bc` 
echo " avg = $tas_avg"

