# $1 starting probability
# $2 ending probability
# $3 step
# $4 number of instances

if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters
		1: starting probability
		2: ending probability
		3: step
		4: number of instances"
	exit 1;
fi


# create a directory for storing benchmark instances


if [ -d "instances" ]; then
        rm -f -r instances/*
else
        mkdir -p instances
fi


	for (( prob=$1; prob <= $2; prob+=$3 ))
	do
		for (( inst=0; inst < $4; inst++ ))
		do
			rp=`printf "%03d" ${prob}`
			in=`printf "%03d" ${inst}`

			# create ontology instances
			./generate.sh ${prob} > "instances/inst_prob_${rp}_inst_${in}.hex"
			cp ontology.owl instances/inst_prob_${rp}_inst_${in}.owl

			# instantiate the program
			cat program.hex | sed "s/OWLONTOLOGY/\"instances\/inst_prob_${rp}_inst_${in}.owl\"/g" >> "instances/inst_prob_${rp}_inst_${in}.hex"

			
		done

	done
