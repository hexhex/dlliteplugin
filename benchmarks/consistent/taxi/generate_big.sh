# $1: number of drivers
# $2: number of regions
# $3: number of customers
# $4: customer probability


prop=$((32768 * $4 / 100)) 
for (( i=1; i <= $3; i++ ))
do
	if [[ $RANDOM -le $prop ]]; then 
		r=$((RANDOM%$2+1))
		echo "needsTo(\"c$i\",\"r$r\")."
		r=$((RANDOM%$2+1))
		echo "isIn(\"c$i\",\"r$r\")."
	fi
done

if [[ $4 -le 33 ]]; then 
	prop1=$((32768 * 30 / 100)) 

elif [[ $4 -le 66 ]]; then 
	prop1=$((32768 * 70 / 100)) 

else 
	prop1=32768 

fi 

for (( i=1; i <= $1; i++ ))
do
	if [[ $RANDOM -le $prop1 ]]; then
		r=$((RANDOM%$2+1))
		echo "isIn(\"d$i\",\"r$r\")."
	fi		
	
	for ((j=1; j<=$2; j++))
		do
			if [[ $RANDOM -le $prop1 ]]; then
				echo "goTo(\"d$i\",\"r$j\")."	
			fi	
		done	
		
done	
	

