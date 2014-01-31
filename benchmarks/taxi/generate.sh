# $1: number of drivers
# $2: number of regions
# $3: number of customers
# $4: edge propability

prop=$((32768 * $4 / 100)) 
for (( i=1; i <= $3; i++ ))
do
	for (( j = 1; j <= $2; j++ ))
	do
		if [[ $RANDOM -le $prop ]]; then
			echo "isIn(\"c$i\",\"r$j\")."
		fi
		if [[ $RANDOM -le $prop ]]; then
			echo "needsTo(\"c$i\",\"r$j\")."
		fi
	done
done

for (( i=1; i <= $1; i++ ))
do
	for (( j = 1; j <= $2; j++ ))
	do
		if [[ $RANDOM -le $prop ]]; then
			echo "isIn(\"d$i\",\"r$j\")."
		fi
		if [[ $RANDOM -le $prop ]]; then
			echo "drivesTo(\"d$i\",\"r$j\")."
		fi
	done
done
