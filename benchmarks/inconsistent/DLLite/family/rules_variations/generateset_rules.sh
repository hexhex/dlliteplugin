# $1 starting probability
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 parameter for the instance difficulty with respect to the data (x: x*50 children, the rest of parameters are proportional) 




if [[ $# -lt 5 ]]; then
        echo "Error: Script expects 5 parameters" 1>&2
	echo "1: starting probability" 1>&2
	echo "2: final probability" 1>&2
	echo "3: step"	1>&2
	echo "4: number of instances" 1>&2
	echo "5: parameter for instance difficulty with respect to the data (x*50 children)" 1>&2
        exit 1;
fi

# prepare a directory for storing benchmark instances

if [ -d "instances" ]; then
        rm instances/*.*
else 
        mkdir -p instances
fi



# instantiate the ontology

s=$5

size=`printf "%03d" ${s}`

./generate_ontology.sh $5 > "ontology_${size}.owl"
./generate_data.sh $5 >"program_${size}.hex"

for (( prop=$1; prop <= $2; prop+=$3 ))
do
        for (( inst=0; inst < $4; inst++ ))
        do
                propf=`printf "%03d" ${prop}`
                in=`printf "%03d" ${inst}`

                # create ontology instances

               	cp ontology_${size}.owl instances/inst_size_${propf}_inst_${in}.owl

                # instantiate the program

		cat program_${size}.hex | sed "s/OWLONTOLOGY/\"instances\/inst_size_${propf}_inst_${in}.owl\"/g" > "instances/inst_size_${propf}_inst_${in}.hex"                
		./generate_rules.sh $prop >> "instances/inst_size_${propf}_inst_${in}.hex"

		

        done
done

rm -f ontology_${size}.owl
rm -f program_${size}.hex
