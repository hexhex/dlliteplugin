# ========== DEFINE CONCEPTS HERE ==========
concepts=(ResearchAssistant PostDoc Student)

# ========== DEFINE ROLES HERE ==========
roles=()



# $1: instance size
# $2: probability p for role assertions (for each pair (ci, cj) of individuals and role R, one of the assertions R(ci, cj) or -R(ci, cj) is added with probability p )

cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions

for (( i=1; i <= $1; i++ ))
do
	# randomly assign the individual to some concept
	c=$((${#concepts[@]} * $RANDOM / 32768))

	# randomly decide if we add it to C or to negC
	pn=$((2 * $RANDOM / 32768))

	# write Abox assertion
	if [[ $pn == 0 ]]; then
		# positive
		echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#${concepts[$c]}\"/></owl:Thing>"
	else
		# negative (TODO: adopt the syntax)
#		echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#${concepts[$c]}\"/></owl:Thing>"
echo  "<owl:Thing rdf:about=\"#c$i\"><rdf:type><owl:Class><owl:complementOf rdf:resource=\"#${concepts[$c]}\"/></owl:Class></rdf:type></owl:Thing>"
	fi

	# for all roles
	for role in ${roles[@]}
	do
		# for all other individuals
		for (( j=1; j <= $1; j++ ))
		do
			# randomly add R(ci, cj), -R(ci, cj), or nothing
			p=$((100 * $RANDOM / 32768))
			if [[ $p -lt $2 ]]; then
				# randomly decide if we add it to C or to negC
				pn=$((2 * $RANDOM / 32768))

				# write Abox assertion
				if [[ $pn == 0 ]]; then
					echo "<owl:Thing rdf:about=\"#c$i\"><$role rdf:resource=\"#c$j\"/></owl:Thing>"
				else
					echo "<owl:NegativePropertyAssertion><owl:assertionProperty rdf:resource=\"#$role\"/><owl:sourceIndividual rdf:resource=\"#c$i\"/><owl:targetIndividual rdf:resource=\"#c$j\"/></owl:NegativePropertyAssertion>"
				fi
			fi
		done
	done
done

cat ontology_footer.owl	# ontology_footer.owl must contain the static part of the owl file which should come after the Abox assertions (closing tags, etc)
