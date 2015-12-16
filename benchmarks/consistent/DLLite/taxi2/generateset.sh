#!/bin/bash

# $1: number of drivers
# $2: number of customers
# $3: number of regions
# $4: number of regions driver goes to
# $5: percentage of ecustomers
# $6: number of instances


if [[ $# -lt 6 ]]; then
	echo "Error: Script expects 6 parameters"
	exit 1;
fi

for (( inst=0; inst < $6; inst++ ))
do
	in=`printf "%03d" ${inst}`

	# create ontology instances
	cp ontology_small.owl instances/inst_size_${propf}_inst_${in}.owl

	# instantiate the program
	./generate_ontology.sh $1 $2 $3 $4 $5 > "instances/taxi_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.owl"

	cat program.hex | sed "s/OWLONTOLOGY/\"instances\/taxi_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.owl\"/g" > "instances/taxi_drv_${1}_cust_${2}_reg_${3}_go_${4}_perc_${5}_inst_${in}.hex"
done
