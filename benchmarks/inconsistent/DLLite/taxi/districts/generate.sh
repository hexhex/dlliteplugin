# $1: instance size
# $2: customer probability

drivers=$(($1*20))
regions=23
customers=$(($1*50))

#echo $drivers
#echo $customers
#echo $regions

prob=$((32768 * $2 / 100)) 
for (( i=1; i <= $customers; i++ ))
do
	if [[ $RANDOM -le $prob ]]; then 
		#r=$((RANDOM%$regions+1))
		echo "isIn(\"c$i\",\"r$((RANDOM%$regions+1))\")."
		echo "needsTo(\"c$i\",\"r$((RANDOM%$regions+1))\")."

	fi
done

if [[ $2 -le 10 ]]; then 
	prob1=$((32768 * 30 / 100)) 

elif [[ $2 -le 20 ]]; then 
	prob1=$((32768 * 70 / 100)) 

else 
	prob1=32768 

fi 

prob2=$((32768 * 50 / 100)) 

for (( i=1; i <= $drivers; i++ ))
do
	if [[ $RANDOM -le $prob2 ]]; then
		r=$((RANDOM%$regions+1))
		echo "isIn(\"d$i\",\"r$r\")."

	for (( j=1; j<=$regions; j++ ))
	do
		if [[ $RANDOM -le $prob2 ]]; then
			echo "goTo(\"d$i\",\"r$j\")."	
		fi	
	done

	fi			
done	
