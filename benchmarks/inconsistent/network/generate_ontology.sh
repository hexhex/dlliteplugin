# ========== DEFINE CONCEPTS HERE ==========
# ========== DEFINE ROLES HERE ==========


cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions


for (( i=1; i <= 69; i++ ))
do
	p=$((RANDOM%100+1))
	if [[ p -le 30 ]];then 
		echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Broken\"/></owl:Thing>"
	else
	echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Avail\"/></owl:Thing>"
	fi
done	




cat ontology_footer.owl	# ontology_footer.owl must contain the static part of the owl file which should come after the Abox assertions (closing tags, etc)
