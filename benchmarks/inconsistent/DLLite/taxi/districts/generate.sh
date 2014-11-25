# $1: instance size
# $2: customer probability

drivers=$(($1*20))
regions=23
customers=$(($1*50))


probcust=$((32768 * $2 / 100)) 
for (( i=1; i <= $customers; i++ ))
do
	if [[ $RANDOM -le $probcust ]]; then 
		echo "isIn(\"c$i\",\"r$((RANDOM%$regions+1))\")."
		echo "needsTo(\"c$i\",\"r$((RANDOM%$regions+1))\")."
	fi
done

probdriv=$(((probcust / 3)+4))

for (( i=1; i <= $drivers; i++ ))
do
	if [[ $RANDOM -le $probdriv ]]; then
		r=$((RANDOM%$regions+1))
		echo "isIn(\"d$i\",\"r$r\")."

	for (( j=1; j<=$regions; j++ ))
	do
		if [[ $RANDOM -le $((probdriv*2)) ]]; then
			echo "goTo(\"d$i\",\"r$j\")."	
		fi	
	done

	fi			
done	
