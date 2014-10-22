# $1 number of individuals

cat ontology_header.owl # ontology_header.owl must contain the static part of the owl file which should come before the Abox assertions

size=$1
# add 50 children
for (( i=1; i <= 50*$1; i++ ))
do
        echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Child\"/></owl:Thing>"
        if [[ $i -ge 43*$1 ]]; then
                echo "<owl:Thing rdf:about=\"#c$i\"><rdf:type rdf:resource=\"#Adopted\"/></owl:Thing>"
        fi
done    


# add 20 male and 20 female parents
for (( i=1; i<=40*$1; i++))
do      
        rem=$(( $i % 2 ))
        if [[ $rem -eq 0 ]]; then 
                echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Female\"/></owl:Thing>"
                        
        else    
                echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Male\"/></owl:Thing>"     

                                
        fi
done





for (( i=41*$1; i<=52*$1; i++))
do      

                echo "<owl:Thing rdf:about=\"#p$i\"><rdf:type rdf:resource=\"#Male\"/></owl:Thing>"
                       
done


# add parent relations
for (( i=1; i <= 50*$1; i++ ))
do
        if [[ $i -le 28*$1 ]]; then
                rem=$(( $i % 2 ))       
                if [[ $rem -eq 0 ]]; then       
                        m=$(($i+1))
                        f=$(($i+2))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
                                                
                else    f=$(($i+1))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$i\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"             
                fi      
        elif [[ $i -ge 29*$1 ]] && [[ $i -le 43*$1 ]]; then
                rem=$(( $(($i-28*$1)) % 2 ))       
                if [[ $rem -eq 0 ]]; then
                        m=$(($i-27*$1))
                        f=$(($i-26*$1))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
                else    f=$(($i-27*$1))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$(($i-28*$1))\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"                     
                fi
        else 
                rem=$(( $(($i-12*$1)) % 2 ))
                if [[ $rem -eq 0 ]]; then
                        m=$(($i-11*$1))
                        f=$(($i-10*$1))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$m\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"
                else    f=$(($i-11*$1))
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$(($i-12*$1))\"/></owl:Thing>"     
                        echo "<owl:Thing rdf:about=\"#c$i\"><hasParent rdf:resource=\"#p$f\"/></owl:Thing>"                     
                fi              
        fi
done    

cat ontology_footer.owl 

