 # $1: number of nodes
 # $2: probability of a fact



prop=$((32768 * $2 / 100)) 


	for (( i=1; i<=$1; i++ ))
	do

		if [[ $RANDOM -le $prop ]]; then
			echo "node(\"c$i\")."
		fi
	done




