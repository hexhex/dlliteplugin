# $1: instance size
# $2: customer probability

drivers=$1*20
regions=$1*5
customers=$1*50


prob=$((32768 * $2 / 100)) 
for (( i=1; i <= $customers; i++ ))
do
	if [[ $RANDOM -le $prob ]]; then 
		r=$((RANDOM%$regions+1))
		echo "needsTo(\"c$i\",\"r$r\")."
		r=$((RANDOM%$regions+1))
		echo "isIn(\"c$i\",\"r$r\")."
	fi
done

if [[ $2 -le 15 ]]; then 
	prob1=$((32768 * 30 / 100)) 

elif [[ $2 -le 30 ]]; then 
	prob1=$((32768 * 50 / 100)) 

elif [[ $2 -le 50 ]]; then 
	prob1=$((32768 * 70 / 100)) 

elif [[ $2 -le 60 ]]; then 
	prob1=$((32768 * 90 / 100)) 
else
	prob1=32768 
fi 

prob2=$((32768 * 50 / 100)) 

for (( i=1; i <= $drivers; i++ ))
do
	if [[ $RANDOM -le $prob1 ]]; then
		r=$((RANDOM%$2+1))
		echo "isIn(\"d$i\",\"r$r\")."

	for ((j=1; j<=$regions; j++))
	do
		if [[ $RANDOM -le $prob2 ]]; then
			echo "goTo(\"d$i\",\"r$j\")."	
		fi	
	done

	fi			
done	
