#$1 (optional) limit support set size or number by repair computation value supsize/supnum
#$2 (optional) bound on support set size (resp. number)

lim=""

if [[ $# == 2 ]] && [[ $1 != "" ]] && [[ $2!="" ]]; then
	lim="--$1=$2"
	echo "Testing partial support families, option $lim is enabled for repair"
fi


start=`date +%s`

loop="instances/*.hex"


# prepare instances folder

if [ -d "instances/consistent" ]; then
	rm -f -r instances/consistent/*

else
	mkdir -p instances/consistent
fi


rm -f instances/*.out

cmd="dlvhex2 --heuristics=monolithic --plugindir=../../../../src --liberalsafety --silent -n=1"

count=`ls -1 instances/*.hex 2>/dev/null | wc -l`



if [ $count != 0 ]; then 

echo "Checking consistency of $count instance(s).."

(echo "number of instances: $count") >> test_result

for instance in $(eval "echo $loop")
do
	$cmd $instance 1>$instance.anset
        hexfilename="$instance"
        owlstring=owl
        ontofilename=$(echo $hexfilename | sed "s/hex/${owlstring}/ g" -)
	asfilename=$(echo $instance.anset)

	#echo "hex file: $hexfilename"
	#echo "owl file: $ontofilename"
		

	if [[ -s $instance.anset ]]; then
		mv  $hexfilename instances/consistent 
		mv  $ontofilename instances/consistent 
		mv  $asfilename instances/consistent
		    
	else
		(echo "$hexfilename") >> test_result
	fi  
done	

else
	echo "There are no files in the instance directory" 
fi


# all consistent instances are moved to the respective folder, and inconsistent ones are left in the instances folder

# we loop over inconsistent instances and compute repairs

incloop="instances/*.hex"

cmdrep="dlvhex2 --heuristics=monolithic --plugindir=../../../../src --supportsets --el --liberalsafety --silent -n=1 --supsize=3"


count=`ls -1 instances/*.hex 2>/dev/null | wc -l`

if [ $count != 0 ]; then 

#remove anset and repinfo files from the instances directory (they all are empty)

	rm -f instances/*.repinfo
	rm -f instances/*.anset

#create a folder for storing repaired instances 

	#if [ -d "instances/repaired" ]; then
	#	rm -f -r instances/repaired/*
	#else
        #	mkdir -p instances/repaired
	#fi

#loop over programs in instance folder and compute repairs

	echo "Repairing $count inconsistent instance(s)..."
	(echo "number of inconsistent instances: $count") >> test_result


	for instance in $(eval "echo $incloop")
	do
		hexfilename="$instance"
		#echo "hex file: $hexfilename"
		owlstring=owl
		ontofilename=$(echo $hexfilename | sed "s/hex/${owlstring}/ g" -)
		#echo "owl file: $ontofilename"

		awk ' BEGIN { print "repair answer set: " } ' >>$instance.repinfo
		$cmdrep $instance --repair=$ontofilename --verbose=1 2>$instance.out >>$instance.repinfo
		awk ' BEGIN { print "******************" } ' >>$instance.repinfo
		
		# check whether repair was found

		cat $instance.out | grep "#RMG:" >$instance.rep
		sed -i 's/\ 1 eO gM #RMG: PC: //g' $instance.rep
		cat $instance.rep >> $instance.repinfo
		awk ' BEGIN { print "******************" } ' >>$instance.repinfo
		rm $instance.rep
		#rm $instance.out
		cp $ontofilename $ontofilename.orig
		cat $instance.repinfo | while read line
		do
			template="aux_o_0_1"

			if [[ "$line" =~ "$template" ]]; then 
				line=$(echo $line | sed s/aux_o_0_1\(\"//g)
				line=$(echo $line | sed s/\"\)//g)
				line=$(echo $line | sed s/\"//g)
				line=$(echo $line | sed s/,/' '/g)
				echo "'$line' is removed from $ontofilename";
				/home/dasha/Documents/software/owl-toolkit/v\-1/owl-toolkit/dist/owl-fact-remove $line $ontofilename
			fi
		done
		
		awk ' BEGIN { print "answer set of a repaired program:" } ' >>$instance.repinfo
		$cmd $instance $lim >>$instance.repinfo
		mv $ontofilename $ontofilename.repaired
		mv $ontofilename.orig $ontofilename


	done
else 
	echo "all instances are consistent"
fi


end=`date +%s`

runtime=$((end-start))

echo "Runtime: $runtime"
