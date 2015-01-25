start=`date +%s`

loop="instances/*.hex"


if [ -d "instances/consistent" ]; then
	rm -f instances/consistent/*.*
else
	mkdir -p instances/consistent
fi

if [ -d "instances/inconsistent" ]; then
	rm -f instances/inconsistent/*.*
else
	mkdir -p instances/inconsistent
fi

rm -f instances/*.out

cmd="dlvhex2 --heuristics=monolithic --plugindir=../../../../../src --supportsets --liberalsafety --silent -n=1"

count=`ls -1 instances/*.hex 2>/dev/null | wc -l`

if [ $count != 0 ]; then 

for instance in $(eval "echo $loop")
do
	$cmd $instance 1>$instance.as
        hexfilename="$instance"
        owlstring=owl
        ontofilename=$(echo $hexfilename | sed "s/hex/${owlstring}/ g" -)

	if [[ -s $instance.as ]]; then
		mv  $hexfilename instances/consistent 
		mv  $ontofilename instances/consistent 
	else
		cp  $hexfilename instances/inconsistent 
		cp  $ontofilename instances/inconsistent
		"$hexfilename" >> test_result 
	fi  
done	

else
	echo "There are no files in the instance directory" 
fi


# now all instances that are left in the instances folder are consistent
incloop="instances/*.hex"

cmdrep="dlvhex2 --heuristics=monolithic --plugindir=../../../../../src --supportsets --liberalsafety --silent -n=1"


if [ -d "instances/instances_repaired" ]; then
	rm instances/instances_repaired/*.*
else
        mkdir -p instances/instances_repaired
fi


for instance in $(eval "echo $incloop")
do
	hexfilename="$instance"
	echo $hexfilename
	owlstring=owl
	ontofilename=$(echo $hexfilename | sed "s/hex/${owlstring}/ g" -)
	echo $ontofilename
	$cmdrep $instance --repair=$ontofilename >out
	
	# check whether the output is empty
	cat $instance.ras | grep "!RMG!:" >rep
	cat $instance.ras | grep "!RMG:">rmg
	sed -i 's/\ 1 gM !RMG!: //g' rep 

	cp $instance instances/instances_repaired
	cat rep | while read line
	do
		line=$(echo $line | sed s/aux_o_0_1\(\"//g)
		line=$(echo $line | sed s/\"\)//g)
		line=$(echo $line | sed s/\"//g)
		line=$(echo $line | sed s/,/' '/g)

	/home/dasha/Documents/software/owl-toolkit/v\-1/owl-toolkit/dist/owl-fact-remove $line instances/inst_size_010_inst_000.owl >instances_repaired/inst_size_010_inst_000_.owl
	done
done


#dlvhex2 --heuristics=monolithic --plugindir=../../../../../src --supportsets --liberalsafety  --repair=instances/inst_size_010_inst_000.owl instances/inst_size_010_inst_000.hex --replimfact=10 --verbose=1 -n=1 --silent 2>out

#cat out | grep "!RMG!:" >rep
#cat out | grep "!RMG:">rmg

#sed -i 's/\ 1 gM !RMG!: //g' rep 

#concept=0
#role=0

#if [ -d "instances_repaired" ]; then
#	rm instances_repaired/*.*
#else 
#	mkdir -p instances_repaired
#fi


#cp instances/inst_size_010_inst_000.hex instances_repaired
#cat rep | while read line
#do

#	line=$(echo $line | sed s/aux_o_0_1\(\"//g)
#	line=$(echo $line | sed s/\"\)//g)
#	line=$(echo $line | sed s/\"//g)
#       line=$(echo $line | sed s/,/' '/g)

	
#/home/dasha/Documents/software/owl-toolkit/v\-1/owl-toolkit/dist/owl-fact-remove $line instances/inst_size_010_inst_000.owl >instances_repaired/inst_size_010_inst_000_.owl


#done

#cat instances_repaired/inst_size_010_inst_000.hex | sed "s/instances/\"instances_repaired\//g" >> "instances_repaired/inst_size_010_inst_000.hex"

end=`date +%s`

runtime=$((end-start))

echo $runtime
