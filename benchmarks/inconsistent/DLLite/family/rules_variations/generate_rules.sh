# $1: rule probability
# $2: maximal number of rules for addition

prop=$((32768 * $1 / 100)) 


for (( i=1; i <= $2; i++ ))
do
	
	if [[ $RANDOM -le $prop ]]; then
		j=$((RANDOM%$2+$i))
		echo "contact$i(X,Y):-contact(X,Y), not omit(X,Y)."
		echo "omit$i(X,Y):-omit(X,Y)."
		echo "contact$j(X,Y):-contact$i(X,Y), not omit$j(X,Y)."	
		echo "omit$j(X,Y):-omit$i(X,Y)."			
	fi
done


