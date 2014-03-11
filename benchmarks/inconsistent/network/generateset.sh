

if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters"
	exit 1;
fi

	for (( concprop=$1; concprop <= $2; concprop+=$3 ))
	do
		for (( inst=0; inst < $4; inst++ ))
		do
			rp=`printf "%03d" ${concprop}`
			in=`printf "%03d" ${inst}`

			# create ontology instances
			./generate_noguess.sh 70 ${concprop} > "instances/inst_prop_${rp}_inst_${in}.hex"
			cp ontology.owl instances/inst_prop_${rp}_inst_${in}.owl

			# instantiate the program
			cat program.hex | sed "s/OWLONTOLOGY/\"inst_prop_${rp}_inst_${in}.owl\"/g" >> "instances/inst_prop_${rp}_inst_${in}.hex"
		done

	done
