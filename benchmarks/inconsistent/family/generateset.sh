if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters"
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
		./generate.sh 50 10 $prop > "instances/inst_size_${propf}_inst_${in}.dlp"
		cp instances/inst_size_${propf}_inst_${in}.dlp instances/inst_size_${propf}_inst_${in}.hex

		cat program.hex | sed "s/OWLONTOLOGY/\"instances\/inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		#cat program.dlp >> "instances/inst_size_${propf}_inst_${in}.dlp"
	done
done
