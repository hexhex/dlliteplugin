for (( nodecount=$1; nodecount <= $2; nodecount+=$3 ))
do
	for (( roleprop=$4; roleprop <= $5; roleprop+=$6 ))
	do
		for (( inst=0; inst < $7; inst++ ))
		do
			ac=`printf "%03d" ${nodecount}`
			rp=`printf "%03d" ${roleprop}`
			in=`printf "%03d" ${inst}`

			# create ontology instances
			./generate.sh $nodecount > "instances/inst_size_${ac}_roleprop_${rp}_inst_${in}.owl"

			# instantiate the program
			cat program.hex | sed "s/OWLONTOLOGY/\"inst_size_${ac}_roleprop_${rp}_inst_${in}.owl\"/g" > "instances/inst_size_${ac}_roleprop_${rp}_inst_${in}.hex"
		done
	done
done
