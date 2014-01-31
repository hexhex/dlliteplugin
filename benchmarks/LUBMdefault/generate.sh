# $1: number of individuals
# $2: probability of a fact


prop=$((32768 * $2 / 100)) 

for (( i=1; i <= $1; i++ ))
do
		if [[ $RANDOM -le $prop ]]; then
			echo "domain(\"c$i\")."
		fi
done

