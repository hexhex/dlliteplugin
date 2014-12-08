#$1 number of nodes
#$2 probability of an edge being forbidden
#$3 probability of a node being broken

rm -f ontology_$1.owl

pbroken=$((32768 * $3 / 100))

./generate_forbid.sh edges_$1 $2 >ontology.owl

echo "avail(X):-&rDL[\"ontology.owl\",a,b,c,d,\"edge\"](X,Y), not &cDL[\"ontology.owl\",a,b,c,d,\"Block\"](X)." >avail.hex

dlvhex2 --liberalsafety --heuristics=monolithic --plugindir=../../../../../src avail.hex --supportsets --ontology=ontology.owl --silent>avail_nodes

sed -i 's/{/''/g' avail_nodes
sed -i 's/}/''/g' avail_nodes
sed -i 's/,/\n/g' avail_nodes

cat ontology.owl >ontology_$1.owl
cat avail_nodes | while read line
do
	line=$(echo $line | sed s/avail\(/''/g)
	line=$(echo $line | sed s/\)/''/g)
	line=$(echo $line | sed s/\"/''/g)

	if [[ $RANDOM -le $pbroken ]]; then
		echo "<owl:Thing rdf:about=\"#$line\"><rdf:type rdf:resource=\"#Broken\"/></owl:Thing>" >>ontology_$1.owl
	else 
		echo "<owl:Thing rdf:about=\"#$line\"><rdf:type rdf:resource=\"#Avail\"/></owl:Thing>" >>ontology_$1.owl
	fi
done


sed -i 's/<\/rdf:RDF>//g' ontology_$1.owl
echo "</rdf:RDF>" >>ontology_$1.owl

/home/dasha/Documents/software/owl-toolkit/v\-1/owl-toolkit/dist/owl-abox-statistics ontology_$1.owl

rm avail_nodes
rm avail.hex
