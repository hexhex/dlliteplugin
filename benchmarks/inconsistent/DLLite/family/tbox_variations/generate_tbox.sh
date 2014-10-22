# $1 probability for a TBox axiom to be added

cat ontology_header.owl # ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions


prop=$((32768 * $1 / 100)) 

#predicates=('Child' 'Male' 'Female')

for (( i=1; i <= 50; i++ ))
do
	
	if [[ $RANDOM -le $prop ]]; then 
		echo "<owl:ObjectProperty rdf:about=\"&family;hasIDOfSocGroup$i\"><rdfs:domain rdf:resource=\"&family;Person\"/></owl:ObjectProperty>"

		echo  "<owl:Class rdf:about=\"&family;ChildMemberOfSocGroup$j\"><rdfs:subClassOf rdf:resource=\"&family;Child\"/></owl:Class>"
		echo  "<owl:Class rdf:about=\"&family;AdoptedMemberOfSocGroup$j\"><rdfs:subClassOf rdf:resource=\"&family;Adopted\"/></owl:Class>"


	fi
done

cat ontology_footer.owl 

