# $1 starting probability
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 parameter for the instance difficulty (x: x*50 children, the rest of parameters are proportional) 
# $6 parameter for presence or absence of domain predicate



if [[ $# -lt 6 ]]; then
        echo "Error: Script expects 6 parameters"
        exit 1;
fi

# instantiate the ontology

s=$5

size=`printf "%03d" ${s}`

./generate_ontology.sh $5 > "ontology_${size}.owl"

for (( prop=$1; prop <= $2; prop+=$3 ))
do
        for (( inst=0; inst < $4; inst++ ))
        do
                propf=`printf "%03d" ${prop}`
                in=`printf "%03d" ${inst}`

                # create ontology instances

                cp ontology_${size}.owl instances/inst_size_${propf}_inst_${in}.owl

                # instantiate the program
                ./generate.sh $5 $6 $prop > "instances/inst_size_${propf}_inst_${in}.hex"

		
		if [[ $6 -eq 1 ]]; then 
                	cat program_domain.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		else 
		 	cat program.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		fi
        done
done

