# $1 starting probability
# $2 finishing probability
# $3 step
# $4 number of instances
## $5 parameter for the instance difficulty with respect to the data (x: x*50 children, the rest of parameters are proportional) 




if [[ $# -lt 4 ]]; then
        echo "Error: Script expects 6 parameters
		1: starting probability
		2: finishing probability
		3: step	
		4: number of instances
		5: parameter for the instance difficulty with respect to the data (x: x*50 children, the rest of parameters are proportional)"
        exit 1;
fi

# create a directory for storing benchmark instances

mkdir -p instances


# instantiate the ontology

s=$5

size=`printf "%03d" ${s}`

#./generate_ontology.sh $5 > "ontology_${size}.owl"

for (( prop=$1; prop <= $2; prop+=$3 ))
do
        for (( inst=0; inst < $4; inst++ ))
        do
                propf=`printf "%03d" ${prop}`
                in=`printf "%03d" ${inst}`

                # create ontology instances

                # cp ontology_${size}.owl instances/inst_size_${propf}_inst_${in}.owl

		cp ontology_001.owl instances/inst_size_${propf}_inst_${in}.owl

                # instantiate the program

		cat program_001.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" > "instances/inst_size_${propf}_inst_${in}.hex"                

		./generate_rules.sh $5 $prop >> "instances/inst_size_${propf}_inst_${in}.hex"

		

        done
done

