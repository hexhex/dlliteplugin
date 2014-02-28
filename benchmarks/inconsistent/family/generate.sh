# $1: number of children
# $2: number of reserved male parents
# $3: children probability
# $4: number of male and female parents 






prop=$((32768 * $3 / 100)) 
for (( i=1; i <= $1; i++ ))
do
	if [[ $RANDOM -le $prop ]]; then 
		echo "boy(\"c$i\")."

		#echo "domain(\"p$i\")."
		#echo "domain(\"p$(($i+1))\")."
		#echo "domain(\"p$(($i+2))\")."
					
		r=$((RANDOM%$2+1))
		s=$(($r+43))		
		echo "ischildof(\"c$i\",\"p$s\")."
		#echo "domain(\"p$(($s))\")."
	fi
done


for (( i=1; i <= $1; i++ ))
do
              echo "domain(\"c$i\")."

                #echo "domain(\"p$i\")."
                #echo "domain(\"p$(($i+1))\")."
                #echo "domain(\"p$(($i+2))\")."

                #r=$((RANDOM%$2+1))
                #s=$(($r+258))
                #echo "ischildof(\"c$i\",\"p$s\")."
                #echo "domain(\"p$(($s))\")."
       
done

for (( i=1; i <= $2; i++ ))
do
              echo "domain(\"p$i\")."

                #echo "domain(\"p$i\")."
                #echo "domain(\"p$(($i+1))\")."
                #echo "domain(\"p$(($i+2))\")."

                #r=$((RANDOM%$2+1))
                #s=$(($r+258))
                #echo "ischildof(\"c$i\",\"p$s\")."
                #echo "domain(\"p$(($s))\")."

done



for (( i=1; i <= $4; i++ ))
do
              echo "domain(\"p$i\")."

                #echo "domain(\"p$i\")."
                #echo "domain(\"p$(($i+1))\")."
                #echo "domain(\"p$(($i+2))\")."

                #r=$((RANDOM%$2+1))
                #s=$(($r+258))
                #echo "ischildof(\"c$i\",\"p$s\")."
                #echo "domain(\"p$(($s))\")."

done




