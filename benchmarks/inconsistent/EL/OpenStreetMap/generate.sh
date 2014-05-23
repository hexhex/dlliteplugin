# $1: probability that a busStop is added to logic program as a fact

prop=$((32768*$1/100)) 


array=( 765215558 721726008 324568673 324385801 1907305831 1078802168 1078802164 )




for j in "${array[@]}"

do

	if [[ $RANDOM -le $prop ]]; then

		echo "busstop(\"$j\")."			
	fi


done


