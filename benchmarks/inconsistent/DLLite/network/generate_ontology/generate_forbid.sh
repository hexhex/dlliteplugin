# /bin/bash
#$1 number of nodes


cat ontology_header.owl


p_forbid=$((32768 * $2 / 100))


cat $1 | while read line
do
	line=$(echo $line | sed s/edge\(/''/g)
	line=$(echo $line | sed s/\)./''/g)
	line=$(echo $line | sed s/,/' '/g)
	line=$(echo $line | sed s/\"/''/g)
	arr=($line)
        n1=${arr[0]}
	n2=${arr[1]}
	
	echo "<owl:Thing rdf:about=\"#$n1\"><edge rdf:resource=\"#$n2\"/></owl:Thing>"

	if [[ $RANDOM -le $p_forbid ]]; then
		echo "<owl:Thing rdf:about=\"#$n2\"><rdf:type rdf:resource=\"#Block\"/></owl:Thing>"
		echo "<owl:Thing rdf:about=\"#$n1\"><forbid rdf:resource=\"#$n2\"/></owl:Thing>"
	fi
done


cat ontology_footer.owl






