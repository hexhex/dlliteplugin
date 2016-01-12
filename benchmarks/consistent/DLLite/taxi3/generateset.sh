#!/bin/bash

# $1: number of drivers
# $2: number of customers
# $3: number of regions
# $4: number of regions driver works in
# $5: percentage of ecustomers and eregions
# $6: number of instances
# $7: instance number


if [[ $# -lt 6 ]]; then
	echo "Error: Script expects 6 parameters"
	exit 1;
fi

for (( inst=0; inst < $6; inst++ ))
do
	in=`printf "%03d" ${inst}`

	# instantiate the program
	./generate_ontology.sh $1 $2 $3 $4 $5 > "instances/taxi_${7}_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.owl"

	cat program.hex | sed "s/OWLONTOLOGY/\"instances\/taxi_${7}_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.owl\"/g" | sed "s/EDRIVER/\"${2}\"/g" > "instances/taxi_${7}_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.hex"
done
