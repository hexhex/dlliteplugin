if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 4 parameters"
	exit 1;
fi



if [ -d "instances" ]; then
        rm -f -r instances/*
else
        mkdir -p instances
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
	
		./generate.sh $prop > "instances/inst_size_${propf}_inst_${in}.dl"
		cp instances/inst_size_${propf}_inst_${in}.dl instances/inst_size_${propf}_inst_${in}.hex
		sed 's/\([a-z][0-9]*\"\)/http\:\/\/www.semanticweb.org\/ontologies\/2014\/4\/policy.owl#\1/g' "instances/inst_size_${propf}_inst_${in}.dl" >"instances/inst_size_${propf}_inst_${in}.dlp"
		cat program.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		

		#cat program.dlp >> "instances/inst_size_${propf}_inst_${in}.dlp"
		#rm instances/*.dl
	done
done
