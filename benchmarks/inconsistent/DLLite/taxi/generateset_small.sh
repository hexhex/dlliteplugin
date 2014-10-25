# $1 starting probabilty
# $2 finishing probability
# $3 step
# $4 number of instances
# $5 size parameter: for n: n*20 drivers, n*5 regions, n*50 customers 


if [[ $# -lt 4 ]]; then
	echo "Error: Script expects 5 parameters"
	exit 1;
fi



# create a directory for storing benchmark instances

mkdir -p instances

for (( prop=$1; prop <= $2; prop+=$3 ))
do
	for (( inst=0; inst < $4; inst++ ))
	do
		propf=`printf "%03d" ${prop}`
		in=`printf "%03d" ${inst}`

		# create ontology instances
		cp ontology_small.owl instances/inst_size_${propf}_inst_${in}.owl

		# instantiate the program
		drivers=20*$5
		regions=5*$5
		customers=50*$5
      		./generate_small.sh $drivers $regions $customers $prop > "instances/inst_size_${propf}_inst_${in}.dlp"
		cp instances/inst_size_${propf}_inst_${in}.dlp instances/inst_size_${propf}_inst_${in}.hex

		cat program.hex | sed "s/OWLONTOLOGY/\"inst_size_${propf}_inst_${in}.owl\"/g" >> "instances/inst_size_${propf}_inst_${in}.hex"
		cat program.dlp >> "instances/inst_size_${propf}_inst_${in}.dlp"
	done
done
