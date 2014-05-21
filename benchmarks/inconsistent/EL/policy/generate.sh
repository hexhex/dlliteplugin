# $1: ontology size
# $2: probability that a certain staff member is owner of a project




prop=$((32768 * $2 / 100)) 

for (( i=1; i <= 40*$2; i++ ))
do
	if [[ $RANDOM -le $prop ]]; then 

		if [[ $i -le 7*$1 ]]; then 
			echo "hasowner(\"p1\",\"s$i\")."
			echo "project(\"p1\")."
		
		elif [[ $i -le 12*$1 ]]; then 	
			echo "hasowner(\"p2\",\"s$i\")."	
			echo "project(\"p2\")."

		elif [[ $i -le 24*$1 ]]; then 	 
			echo "hasowner(\"p3\",\"s$i\")."		
			echo "project(\"p3\")."		

		elif [[ $i -le 31*$1 ]]; then
			echo "hasowner(\"p4\",\"s$i\")."		 	
			echo "project(\"p4\")."

		else 
			echo "hasowner(\"p5\",\"s$i\")."
			echo "project(\"p5\")."
		
		fi	
	fi
done



