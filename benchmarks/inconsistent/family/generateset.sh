# $1 starting probability
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 parameter for the intsnace difficulty 
# $6 parameter for presentce of babsence of domain predicate


if [[ $# -lt 6 ]]; then
        echo "Error: Script expects 6 parameters"
        exit 1;
fi

for (( prop=$1; prop <= $2; prop+=$3 ))
do
        for (( inst=0; inst < $4; inst++ ))
        do
                propf=`printf "%03d" ${prop}`
                in=`printf "%03d" ${inst}`

                # create ontology instances
                cp ontology.owl instances/inst_size_${propf}_inst_${in}.owl

                # instantiate the program
                ./generate.sh $5 $6 $prop > "instances/inst_size_${propf}_inst_${in}.hex"

		if [[ $6 -eq 1 ]]; then 
                	cat program_domain.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		else 
		 	cat program.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		fi
        done
done

