 # $1: number of nodes
 # $2: probability of a fact

in=( 1 3 6 9 10 11 12 13 14 15 17 18 19 20 23 26 28 29 30 32 34 41 46 47 48 50 53 56 60 61 62 63 68 67 )

out=( 2 4 5 7 8 16 21 22 24 25 27 31 33 35 36 37 38 39 40 42 43 44 45 49 51 52 54 55 57 58 59 64 65 66 69 70 )


propin=$((32768 * $2 / 100))


if [[ $propin -le 20 ]]; then
	temp=$(($2*4))
	propout=$((32768 * $temp / 100)) 
elif [[ $propin -le 30 ]]; then 
	temp=$(($2*3))
        propout=$((32768 * $temp / 100))
elif [[ $propin -le 40 ]]; then
	temp=$((($4*2)+10))
        propout=$((32768 * $temp / 100))
else 
	propout=32768
fi

for i in "${in[@]}" 
do
	if [[ $RANDOM -le $propin ]]; then
		echo "in(\"c$i\")."
	fi
done

 for i in "${out[@]}" 
 do
        if [[ $RANDOM -le $propout ]]; then
                 echo "out(\"c$i\")."
        fi
 done



