# $1: parameter, if it is 1, then 50 children (among them 7 adopted), 20 female, 32 male
# $2: domain presence
# $3: children probability




prop=$((32768 * $3 / 100)) 
for (( i=1; i <= 50*$1; i++ ))
do
	if [[ $RANDOM -le $prop ]]; then 
		echo "boy(\"c$i\")."			
		r=$((RANDOM%12*$1+1))
		s=$(($r+43*$1))		
		echo "ischildof(\"c$i\",\"p$s\")."
	fi
done


if [[ $2 -eq 1 ]]; then 

	for (( i=1; i <= 50*$1; i++ ))
	do
              echo "domain(\"c$i\")."
	done



	for (( i=1; i <= 52*$1; i++ ))
	do
              echo "domain(\"p$i\")."
	done
fi
