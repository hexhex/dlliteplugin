# $1 instance size

cat ontology_header.owl # ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions


size=$1
customers=$((50*$size))
drivers=$((20*$size))
regions=23

# add 50*$1 customers
for (( i=1; i <= $customers; i++ ))
do
        echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Customer\"/></owl:Thing>"
done    


edrivprob=$((32768 * 85 / 100))
worksinprob=$((32768 * 17 / 100))


for (( i=1; i <= $drivers; i++ ))
do
        echo "<owl:Thing rdf:about=\"#d$i\"><rdf:type rdf:resource=\"#Driver\"/></owl:Thing>"
	
	# with probability 0.75 driver is EDriver
	if [[ $RANDOM -le $edrivprob ]]; then 
		echo "<owl:Thing rdf:about=\"#d$i\"><rdf:type rdf:resource=\"#EDriver\"/></owl:Thing>"
	fi

	# with probability 0.15 driver works in a certain region
	for (( j=1; j<= $regions; j++ ))
	do
		if [[ $RANDOM -le $worksinprob ]]; then 
			echo  "<owl:Thing rdf:about=\"#d$i\"><worksIn rdf:resource=\"#r$j\"/></owl:Thing>"
		else 
			echo  "<owl:Thing rdf:about=\"#d$i\"><notworksIn rdf:resource=\"#r$j\"/></owl:Thing>"
		fi
		
        done
done    




cat ontology_footer.owl 

