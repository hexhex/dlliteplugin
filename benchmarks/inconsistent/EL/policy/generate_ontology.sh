if [[ $# -lt 1 ]]; then
	echo "Error: Script expects 5 parameters"
	exit 1;
fi
cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions



for (( i=1; i <= 5*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Staff\"/></owl:Thing>"
done


for (( i=1; i <= 1*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Unauthorized\"/></owl:Thing>"
done



for (( i=2; i <= 2*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Blacklisted\"/></owl:Thing>"
done


for (( i=1; i <= 2*$1; i++ ))
do


ac=$((RANDOM%3+1))

	echo "<owl:Thing rdf:about=\"#r$i\"><rdf:type rdf:resource=\"#StaffRequest\"/></owl:Thing>"
	echo "<owl:Thing rdf:about=\"#r$i\"><hasSubject rdf:resource=\"#s$i\"/></owl:Thing>"
	echo "<owl:Thing rdf:about=\"#r$i\"><hasAction rdf:resource=\"#a$ac\"/></owl:Thing>"
	if [[ $i -le 1*$1 ]]; then
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p1\"/></owl:Thing>"	

	elif [[ $i -le 2*$1 ]]; then
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p2\"/></owl:Thing>"	

	elif [[ $i -le 3*$1 ]]; then 
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p3\"/></owl:Thing>"	

	elif [[ $i -le 4*$1 ]]; then 
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p4\"/></owl:Thing>"	

	else  echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p5\"/></owl:Thing>"	

	fi	
done	

for (( i=1; i <= 5; i++ ))
do

	echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Project\"/></owl:Thing>"

done

for (( i=1; i <= 3; i++ ))
do

	echo "<owl:Thing rdf:about=\"#a$i\"><rdf:type rdf:resource=\"#Action\"/></owl:Thing>"

done


cat ontology_footer.owl	
