#!/bin/bash

# $1: number of drivers
# $2: number of customers
# $3: number of regions
# $4: number of regions driver works in
# $5: percentage of ecustomers and eregions

if [[ $# -lt 5 ]]; then
	echo "Error: Script expects 5 parameter"
	exit 1;
fi

cat ontology_header.owl

is=1

for (( i=1; i <= $1; i++ ))
do
	edrv=$(( 32768 / 2 ))

	echo "    <owl:Thing rdf:about=\"#d$i\"><rdf:type rdf:resource=\"#Driver\"/></owl:Thing>"

	
	echo "    <owl:Thing rdf:about=\"#d$i\"><isIn rdf:resource=\"#r$is\"/></owl:Thing>"
	is=$(( $is + 1 ))

	if [[ $is -ge $3+1 ]]
	then
		is=1
	fi

	for (( m=1; m <= $4; m++ ))
	do
		works=$(( ($RANDOM%($3))+1 ))
		echo "    <owl:Thing rdf:about=\"#d$i\"><worksIn rdf:resource=\"#r$works\"/></owl:Thing>"
	done
done

prop=$((32768 * $5 / 100)) 

for (( i=1; i <= $2; i++ ))
do
	if [[ $RANDOM -le $prop ]]
	then
		echo "    <owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#ECustomer\"/></owl:Thing>"
	else
		echo "    <owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Customer\"/></owl:Thing>"
	fi

	is=$(( ($RANDOM%($3))+1 ))
	echo "    <owl:Thing rdf:about=\"#c$i\"><isIn rdf:resource=\"#r$is\"/></owl:Thing>"
done


prop=$((32768 * $5 / 100)) 
for (( i=1; i <= $3; i++ ))
do
	if [[ $RANDOM -le $prop ]]
	then
		echo "    <owl:Thing rdf:about=\"#r$i\"><rdf:type rdf:resource=\"#ERegion\"/></owl:Thing>"
	else
		echo "    <owl:Thing rdf:about=\"#r$i\"><rdf:type rdf:resource=\"#Region\"/></owl:Thing>"
	fi
done

cat ontology_footer.owl

