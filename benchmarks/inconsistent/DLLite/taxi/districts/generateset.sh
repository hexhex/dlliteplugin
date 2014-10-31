# $1 starting probabilty
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 size parameter: for n: n*20 drivers, n*5 regions, n*50 customers 


if [[ $# -lt 4 ]]; then
        echo "Error: Script expects 4 parameters
		1: starting probability 
		2: finishing probability
		3: step	
		4: number of instances
		5: instance size"
        exit 1;
fi


# create a directory for storing benchmark instances

if [ -d "instances" ]; then
	rm instances/*.*
else 
	mkdir -p instances
fi

#./generate_ontology.sh $5 > ontology.owl


for (( prob=$1; prob <= $2; prob+=$3 ))
do
	for (( inst=0; inst < $4; inst++ ))
	do
		probf=`printf "%03d" ${prob}`
		in=`printf "%03d" ${inst}`

		# create ontology instances
		cp ontology.owl instances/inst_size_${probf}_inst_${in}.owl

		# instantiate the program
      		./generate.sh $5 $prob > "instances/inst_size_${probf}_inst_${in}.hex"


		cat program.hex | sed "s/OWLONTOLOGY/\"instances\/inst_size_${probf}_inst_${in}.owl\"/g" >> "instances/inst_size_${probf}_inst_${in}.hex"
	
	done
done
