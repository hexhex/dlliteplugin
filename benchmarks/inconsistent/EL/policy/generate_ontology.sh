if [[ $# -lt 1 ]]; then
	echo "Error: Script expects 5 parameters"
	exit 1;
fi
cat ontology_header.owl	# ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions



for (( i=1; i <= 100*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Staff\"/></owl:Thing>"
done


for (( i=1; i <= 21*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Unauthorized\"/></owl:Thing>"
done



for (( i=22; i <= 36*$1; i++ ))
do
	echo "<owl:Thing rdf:about=\"#s$i\"><rdf:type rdf:resource=\"#Blacklisted\"/></owl:Thing>"
done


for (( i=1; i <= 36*$1; i++ ))
do


a1=$((RANDOM%3+1))

	echo "<owl:Thing rdf:about=\"#r$i\"><rdf:type rdf:resource=\"#StaffRequest\"/></owl:Thing>"
	echo "<owl:Thing rdf:about=\"#r$i\"><hasSubject rdf:resource=\"#s$i\"/></owl:Thing>"
	echo "<owl:Thing rdf:about=\"#r$i\"><hasAction rdf:resource=\"#a$a1\"/></owl:Thing>"
	if [[ $i -le 7*$1 ]]; then
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p1\"/></owl:Thing>"	

	elif [[ $i -le 12*$1 ]]; then
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p2\"/></owl:Thing>"	

	elif [[ $i -le 24*$1 ]]; then 
		echo "<owl:Thing rdf:about=\"#r$i\"><hasTarget rdf:resource=\"#p3\"/></owl:Thing>"	

	elif [[ $i -le 31*$1 ]]; then 
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
