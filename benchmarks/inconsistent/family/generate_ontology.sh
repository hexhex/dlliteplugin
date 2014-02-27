# ========== DEFINE CONCEPTS HERE ==========
# ========== DEFINE ROLES HERE ==========


cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions

# add 50 children
for (( i=1; i <= 50; i++ ))
do
	echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Child\"/></owl:Thing>"
	#echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Domain\"/></owl:Thing>"
	if [[ $i -ge 43 ]]; then
		echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Adopted\"/></owl:Thing>"
	fi
done	


# add 20 male and 20 female parents
for (( i=1; i<=40; i++))
do	
	rem=$(( $i % 2 ))
	if [[ $rem -eq 0 ]]; then 
		echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Female\"/></owl:Thing>"
			
	else 	
		echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Male\"/></owl:Thing>"	

				
	fi
		#echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Domain\"/></owl:Thing>"
done





for (( i=41; i<=52; i++))
do	
#	rem=$(( $i % 2 ))

		echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Male\"/></owl:Thing>"
		#echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Domain\"/></owl:Thing>"
			
done


# add parent relations
for (( i=1; i <= 50; i++ ))
do
	if [[ $i -le 28 ]]; then
		rem=$(( $i % 2 ))	
		if [[ $rem -eq 0 ]]; then 	
			m=$(($i+1))
			f=$(($i+2))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
						
                else    f=$(($i+1))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$i\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"		
		fi	
	elif [ $i -ge 29 ] && [ $i -le 43 ]; then
		rem=$(( $(($i-28)) % 2 ))	
		if [[ $rem -eq 0 ]]; then
			m=$(($i-27))
			f=$(($i-26))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
		else    f=$(($i-27))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$(($i-28))\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"			
		fi
	else 
		rem=$(( $(($i-12)) % 2 ))
		if [[ $rem -eq 0 ]]; then
			m=$(($i-11))
			f=$(($i-10))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
		else    f=$(($i-11))
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$(($i-12))\"/></owl:Thing>"	
			echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"			
		fi		
	fi
done	

cat ontology_footer.owl	
# ontology_footer.owl must contain the static part of the owl file which should come after the Abox assertions (closing tags, etc)
