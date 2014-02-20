# $1: number of children
# $2: number of reserved male parents
# $3: children probability


prop=$((32768 * $3 / 100)) 
for (( i=1; i <= $1; i++ ))
do
	if [[ $RANDOM -le $prop ]]; then 
		echo "boy(\"c$i\")."
		echo "domain(\"c$i\")."
		echo "domain(\"p$i\")."
		echo "domain(\"p$(($i+1))\")."
		echo "domain(\"p$(($i+2))\")."
					
		r=$((RANDOM%$2+1))
		s=$(($r+42))		
		echo "ischildof(\"c$i\",\"p$s\")."
		echo "domain(\"p$(($s))\")."
	fi
done

