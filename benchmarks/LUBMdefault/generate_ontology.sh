# ========== DEFINE CONCEPTS HERE ==========
concepts=(ResearchAssistant Student Employee)

# ========== DEFINE ROLES HERE ==========


cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions


for (( i=1; i <= $1; i++ ))
do
pn=$((3 * $RANDOM / 32768))

	echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#${concepts[$pn]}\"/></owl:Thing>"
	
done	





cat ontology_footer.owl	# ontology_footer.owl must contain the static part of the owl file which should come after the Abox assertions (closing tags, etc)
