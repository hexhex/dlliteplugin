# $1 starting probability
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 size of the data part (generated data part is the same for all instances)




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
else mkdir -p instances
fi



# instantiate the ontology
s=$5


size=`printf "%03d" ${s}`

rm -f abox_${size}.owl
rm -f program_${size}.hex

./generate_abox.sh $5 >"abox_${size}.owl"
./generate_program.sh $5 >"program_${size}.hex"

for (( prop=$1; prop <= $2; prop+=$3 ))
do
        for (( inst=0; inst < $4; inst++ ))
        do
                propf=`printf "%03d" ${prop}`
                in=`printf "%03d" ${inst}`

                # create ontology instances

                ./generate_ontology.sh $prop "abox_${size}.owl" > instances/inst_size_${propf}_inst_${in}.owl

                # instantiate the progra

		cat program_${size}.hex | sed "s/OWLONTOLOGY/\"instances\/inst_size_${propf}_inst_${in}.owl\"/g" > "instances/inst_size_${propf}_inst_${in}.hex"  
        done
done

                rm -f program_${size}.hex
                rm -f abox_${size}.owl  

