#!/bin/bash

# $1: number of drivers
# $2: number of customers
# $3: number of regions
# $4: number of regions driver goes to
# $5: percentage of ecustomers

if [[ $# -lt 5 ]]; then
	echo "Error: Script expects 5 parameter"
	exit 1;
fi

cat ontology_header.owl


for (( i=1; i <= $1; i++ ))
do
	edrv=$(( 32768 / 2 ))
	if [[ $RANDOM -le $edrv ]]
	then
		echo "    <owl:Thing rdf:about=\"#d$i\"><rdf:type rdf:resource=\"#Driver\"/></owl:Thing>"
	else
		echo "    <owl:Thing rdf:about=\"#d$i\"><rdf:type rdf:resource=\"#EDriver\"/></owl:Thing>"
	fi
	for (( m=1; m <= $4; m++ ))
	do
		goes=$(( ($RANDOM%($3))+1 ))
		echo "    <owl:Thing rdf:about=\"#d$i\"><goesTo rdf:resource=\"#r$goes\"/></owl:Thing>"
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

	needs=$(( ($RANDOM%($3))+1 ))
	echo "    <owl:Thing rdf:about=\"#c$i\"><needsTo rdf:resource=\"#r$needs\"/></owl:Thing>"
done

for (( i=1; i <= $3; i++ ))
do
	echo "    <owl:Thing rdf:about=\"#r$i\"><rdf:type rdf:resource=\"#Region\"/></owl:Thing>"
done

cat ontology_footer.owl

