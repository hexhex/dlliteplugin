 # $1: probability of a fact




array1=( 1 2 3 9 8 5 6 7 10 47 48 49 60 61 51 52 53 55 70)

array2=( 54 34 62 63 11 65 64 66 67 68 69 56 57 36 35 )

array3=( 16 46 45 15 12 16 17 43 44 42 37 38  39 40 22)

array4=( 22 21 18 19 20 25 26 59 58 14 32 31 27 28 29)




prop=$((32768 * $1 / 100)) 

r=$((RANDOM%4+1))

 if [[ $r -eq 1 ]]; then

	for j in "${array1[@]}"
	do
		echo "domain(\"c$j\")."
	
		if [[ $RANDOM -le $prop ]]; then
			echo "node(\"c$j\")."
		fi
	done

elif [[ $r -eq 2 ]]; then

	for j in "${array2[@]}"
	do
		echo "domain(\"c$j\")."
	
		if [[ $RANDOM -le $prop ]]; then
			echo "node(\"c$j\")."
		fi
	done

elif [[ $r -eq 3 ]]; then

	for j in "${array3[@]}"
	do
		echo "domain(\"c$j\")."
	
		if [[ $RANDOM -le $prop ]]; then
			echo "node(\"c$j\")."
		fi
	done

else 
	for j in "${array4[@]}"
	do
		echo "domain(\"c$j\")."
	
		if [[ $RANDOM -le $prop ]]; then
			echo "node(\"c$j\")."
		fi
	done

fi






