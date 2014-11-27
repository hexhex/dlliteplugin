# $1: probability of a fact

concepts=( "GraduateStudent131" "GraduateStudent6" "GraduateStudent0" "GraduateStudent75" "GraduateStudent117" "GraduateStudent29" "GraduateStudent11" "GraduateStudent42" "GraduateStudent92" "GraduateStudent12" "GraduateStudent121" "GraduateStudent132" "GraduateStudent73" "GraduateStudent137" "GraduateStudent85" "GraduateStudent68" "GraduateStudent122" "GraduateStudent62" "GraduateStudent91" "GraduateStudent104" "GraduateStudent39" "GraduateStudent35" "GraduateStudent64" "GraduateStudent3" "GraduateStudent70" "GraduateStudent10" "GraduateStudent8" "GraduateStudent55" "GraduateStudent80" "GraduateStudent135" "GraduateStudent128" "GraduateStudent38" "GraduateStudent21" "GraduateStudent56" "GraduateStudent105" "GraduateStudent112" "GraduateStudent25" "GraduateStudent88" "GraduateStudent98" "AssistantProfessor0" "AssistantProfessor1" "AssistantProfessor2" "AssistantProfessor3" "AssistantProfessor4" "AssistantProfessor5" "AssistantProfessor6" "AssistantProfessor7" "AssistantProfessor8" "AssistantProfessor9" "AssociateProfessor0" "AssociateProfessor1" "AssociateProfessor10" "AssociateProfessor11" "AssociateProfessor12" "AssociateProfessor13" "AssociateProfessor2" "AssociateProfessor3" "AssociateProfessor4" "AssociateProfessor5" "AssociateProfessor6" "AssociateProfessor7" "AssociateProfessor8" "AssociateProfessor9" "FullProfessor7" "FullProfessor0" "FullProfessor1" "FullProfessor2" "FullProfessor3" "FullProfessor4" "FullProfessor5" "FullProfessor6" "FullProfessor8" "FullProfessor9" "Lecturer0" "Lecturer1" "Lecturer2" "Lecturer3" "Lecturer4" "Lecturer5" "Lecturer6" "GraduateStudent131" "GraduateStudent6" "GraduateStudent0" "GraduateStudent75" "GraduateStudent117" "GraduateStudent29" "GraduateStudent11" "GraduateStudent42" "GraduateStudent92" "GraduateStudent12" "GraduateStudent121" "GraduateStudent132" "GraduateStudent73" "GraduateStudent137" "GraduateStudent85" "GraduateStudent68" "GraduateStudent122" "GraduateStudent62" "GraduateStudent91" "GraduateStudent104" "GraduateStudent39" "GraduateStudent35" "GraduateStudent64" "GraduateStudent3" "GraduateStudent70" "GraduateStudent10" "GraduateStudent8" "GraduateStudent55" "GraduateStudent80" "GraduateStudent135" "GraduateStudent128" "GraduateStudent38" "GraduateStudent21" "GraduateStudent56" "GraduateStudent105" "GraduateStudent112" "GraduateStudent25" "GraduateStudent88" "GraduateStudent98" "UndergraduateStudent0" "UndergraduateStudent1" "UndergraduateStudent10" "UndergraduateStudent100" "UndergraduateStudent101" "UndergraduateStudent102" "UndergraduateStudent103" "UndergraduateStudent104" "UndergraduateStudent105" "UndergraduateStudent106" "UndergraduateStudent107" "UndergraduateStudent108" "UndergraduateStudent109" "UndergraduateStudent11" "UndergraduateStudent110" "UndergraduateStudent111" "UndergraduateStudent112" "UndergraduateStudent113" "UndergraduateStudent114" "UndergraduateStudent115" "UndergraduateStudent116" "UndergraduateStudent117" "UndergraduateStudent118" "UndergraduateStudent119" "UndergraduateStudent12" "UndergraduateStudent120" "UndergraduateStudent121" "UndergraduateStudent122" "UndergraduateStudent123" "UndergraduateStudent124" "UndergraduateStudent125" "UndergraduateStudent126" "UndergraduateStudent127" "UndergraduateStudent128" "UndergraduateStudent129" "UndergraduateStudent13" "UndergraduateStudent130" "UndergraduateStudent131" "UndergraduateStudent132" "UndergraduateStudent133" "UndergraduateStudent134" "UndergraduateStudent135" "UndergraduateStudent136" "UndergraduateStudent137" "UndergraduateStudent138" "UndergraduateStudent139" "UndergraduateStudent14" "UndergraduateStudent140" "UndergraduateStudent141" "UndergraduateStudent142" "UndergraduateStudent143" "UndergraduateStudent144" "UndergraduateStudent145" "UndergraduateStudent146" "UndergraduateStudent147" "UndergraduateStudent148" "UndergraduateStudent149" "UndergraduateStudent15" "UndergraduateStudent150" "UndergraduateStudent151" "UndergraduateStudent152" "UndergraduateStudent153" "UndergraduateStudent154" "UndergraduateStudent155" "UndergraduateStudent156" "UndergraduateStudent157" "UndergraduateStudent158" "UndergraduateStudent159" "UndergraduateStudent16" "UndergraduateStudent160" "UndergraduateStudent161" "UndergraduateStudent162" "UndergraduateStudent163" "UndergraduateStudent164" "UndergraduateStudent165" "UndergraduateStudent166" "UndergraduateStudent167" "UndergraduateStudent168" "UndergraduateStudent169" "UndergraduateStudent17" "UndergraduateStudent170" "UndergraduateStudent171" "UndergraduateStudent172" "UndergraduateStudent173" "UndergraduateStudent174" "UndergraduateStudent175" "UndergraduateStudent176" "UndergraduateStudent177" "UndergraduateStudent178" "UndergraduateStudent179" "UndergraduateStudent18" "UndergraduateStudent180" "UndergraduateStudent181" "UndergraduateStudent182" "UndergraduateStudent183" "UndergraduateStudent184" "UndergraduateStudent185" "UndergraduateStudent186" "UndergraduateStudent187" "UndergraduateStudent188" "UndergraduateStudent189" "UndergraduateStudent19" "UndergraduateStudent190" "UndergraduateStudent191" "UndergraduateStudent192" "UndergraduateStudent193" "UndergraduateStudent194" "UndergraduateStudent195" "UndergraduateStudent196" "UndergraduateStudent197" "UndergraduateStudent198" "UndergraduateStudent199" "UndergraduateStudent2" "UndergraduateStudent20" "UndergraduateStudent200" "UndergraduateStudent201" "UndergraduateStudent202" "UndergraduateStudent203" "UndergraduateStudent204" "UndergraduateStudent205" "UndergraduateStudent206" "UndergraduateStudent207" "UndergraduateStudent208" "UndergraduateStudent209" "UndergraduateStudent21" "UndergraduateStudent210" "UndergraduateStudent211" "UndergraduateStudent212" "UndergraduateStudent213" "UndergraduateStudent214" "UndergraduateStudent215" "UndergraduateStudent216" "UndergraduateStudent217" "UndergraduateStudent218" "UndergraduateStudent219" "UndergraduateStudent22" "UndergraduateStudent220" "UndergraduateStudent221" "UndergraduateStudent222" "UndergraduateStudent223" "UndergraduateStudent224" "UndergraduateStudent225" "UndergraduateStudent226" "UndergraduateStudent227" "UndergraduateStudent228" "UndergraduateStudent229" "UndergraduateStudent23" "UndergraduateStudent230" "UndergraduateStudent231" "UndergraduateStudent232" "UndergraduateStudent233" "UndergraduateStudent234" "UndergraduateStudent235" "UndergraduateStudent236" "UndergraduateStudent237" "UndergraduateStudent238" "UndergraduateStudent239" "UndergraduateStudent24" "UndergraduateStudent240" "UndergraduateStudent241" "UndergraduateStudent242" "UndergraduateStudent243" "UndergraduateStudent244" "UndergraduateStudent245" "UndergraduateStudent246" "UndergraduateStudent247" "UndergraduateStudent248" "UndergraduateStudent249" "UndergraduateStudent25" "UndergraduateStudent250" "UndergraduateStudent251" "UndergraduateStudent252" "UndergraduateStudent253" "UndergraduateStudent254" "UndergraduateStudent255" "UndergraduateStudent256" "UndergraduateStudent257" "UndergraduateStudent258" "UndergraduateStudent259" "UndergraduateStudent26" "UndergraduateStudent260" "UndergraduateStudent261" "UndergraduateStudent262" "UndergraduateStudent263" "UndergraduateStudent264" "UndergraduateStudent265" "UndergraduateStudent266" "UndergraduateStudent267" "UndergraduateStudent268" "UndergraduateStudent269" "UndergraduateStudent27" "UndergraduateStudent270" "UndergraduateStudent271" "UndergraduateStudent272" "UndergraduateStudent273" "UndergraduateStudent274" "UndergraduateStudent275" "UndergraduateStudent276" "UndergraduateStudent277" "UndergraduateStudent278" "UndergraduateStudent279" "UndergraduateStudent28" "UndergraduateStudent280" "UndergraduateStudent281" "UndergraduateStudent282" "UndergraduateStudent283" "UndergraduateStudent284" "UndergraduateStudent285" "UndergraduateStudent286" "UndergraduateStudent287" "UndergraduateStudent288" "UndergraduateStudent289" "UndergraduateStudent29" "UndergraduateStudent290" "UndergraduateStudent291" "UndergraduateStudent292" "UndergraduateStudent293" "UndergraduateStudent294" "UndergraduateStudent295" "UndergraduateStudent296" "UndergraduateStudent297" "UndergraduateStudent298" "UndergraduateStudent299" "UndergraduateStudent3" "UndergraduateStudent30" "UndergraduateStudent300" "UndergraduateStudent301" "UndergraduateStudent302" "UndergraduateStudent303" "UndergraduateStudent304" "UndergraduateStudent305" "UndergraduateStudent306" "UndergraduateStudent307" "UndergraduateStudent308" "UndergraduateStudent309" "UndergraduateStudent31" "UndergraduateStudent310" "UndergraduateStudent311" "UndergraduateStudent312" "UndergraduateStudent313" "UndergraduateStudent314" "UndergraduateStudent315" "UndergraduateStudent316" "UndergraduateStudent317" "UndergraduateStudent318" "UndergraduateStudent319" "UndergraduateStudent32" "UndergraduateStudent320" "UndergraduateStudent321" "UndergraduateStudent322" "UndergraduateStudent323" "UndergraduateStudent324" "UndergraduateStudent325" "UndergraduateStudent326" "UndergraduateStudent327" "UndergraduateStudent328" "UndergraduateStudent329" "UndergraduateStudent33" "UndergraduateStudent330" "UndergraduateStudent331" "UndergraduateStudent332" "UndergraduateStudent333" "UndergraduateStudent334" "UndergraduateStudent335" "UndergraduateStudent336" "UndergraduateStudent337" "UndergraduateStudent338" "UndergraduateStudent339" "UndergraduateStudent34" "UndergraduateStudent340" "UndergraduateStudent341" "UndergraduateStudent342" "UndergraduateStudent343" "UndergraduateStudent344" "UndergraduateStudent345" "UndergraduateStudent346" "UndergraduateStudent347" "UndergraduateStudent348" "UndergraduateStudent349" "UndergraduateStudent35" "UndergraduateStudent350" "UndergraduateStudent351" "UndergraduateStudent352" "UndergraduateStudent353" "UndergraduateStudent354" "UndergraduateStudent355" "UndergraduateStudent356" "UndergraduateStudent357" "UndergraduateStudent358" "UndergraduateStudent359" "UndergraduateStudent36" "UndergraduateStudent360" "UndergraduateStudent361" "UndergraduateStudent362" "UndergraduateStudent363" "UndergraduateStudent364" "UndergraduateStudent365" "UndergraduateStudent366" "UndergraduateStudent367" "UndergraduateStudent368" "UndergraduateStudent369" "UndergraduateStudent37" "UndergraduateStudent370" "UndergraduateStudent371" "UndergraduateStudent372" "UndergraduateStudent373" "UndergraduateStudent374" "UndergraduateStudent375" "UndergraduateStudent376" "UndergraduateStudent377" "UndergraduateStudent378" "UndergraduateStudent379" "UndergraduateStudent38" "UndergraduateStudent380" "UndergraduateStudent381" "UndergraduateStudent382" "UndergraduateStudent383" "UndergraduateStudent384" "UndergraduateStudent385" "UndergraduateStudent386" "UndergraduateStudent387" "UndergraduateStudent388" "UndergraduateStudent389" "UndergraduateStudent39" "UndergraduateStudent390" "UndergraduateStudent391" "UndergraduateStudent392" "UndergraduateStudent393" "UndergraduateStudent394" "UndergraduateStudent395" "UndergraduateStudent396" "UndergraduateStudent397" "UndergraduateStudent398" "UndergraduateStudent399" "UndergraduateStudent4" "UndergraduateStudent40" "UndergraduateStudent400" "UndergraduateStudent401" "UndergraduateStudent402" "UndergraduateStudent403" "UndergraduateStudent404" "UndergraduateStudent405" "UndergraduateStudent406" "UndergraduateStudent407" "UndergraduateStudent408" "UndergraduateStudent409" "UndergraduateStudent41" "UndergraduateStudent410" "UndergraduateStudent411" "UndergraduateStudent412" "UndergraduateStudent413" "UndergraduateStudent414" "UndergraduateStudent415" "UndergraduateStudent416" "UndergraduateStudent417" "UndergraduateStudent418" "UndergraduateStudent419" "UndergraduateStudent42" "UndergraduateStudent420" "UndergraduateStudent421" "UndergraduateStudent422" "UndergraduateStudent423" "UndergraduateStudent424" "UndergraduateStudent425" "UndergraduateStudent426" "UndergraduateStudent427" "UndergraduateStudent428" "UndergraduateStudent429" "UndergraduateStudent43" "UndergraduateStudent430" "UndergraduateStudent431" "UndergraduateStudent432" "UndergraduateStudent433" "UndergraduateStudent434" "UndergraduateStudent435" "UndergraduateStudent436" "UndergraduateStudent437" "UndergraduateStudent438" "UndergraduateStudent439" "UndergraduateStudent44" "UndergraduateStudent440" "UndergraduateStudent441" "UndergraduateStudent442" "UndergraduateStudent443" "UndergraduateStudent444" "UndergraduateStudent445" "UndergraduateStudent446" "UndergraduateStudent447" "UndergraduateStudent448" "UndergraduateStudent449" "UndergraduateStudent45" "UndergraduateStudent450" "UndergraduateStudent451" "UndergraduateStudent452" "UndergraduateStudent453" "UndergraduateStudent454" "UndergraduateStudent455" "UndergraduateStudent456" "UndergraduateStudent457" "UndergraduateStudent458" "UndergraduateStudent459" "UndergraduateStudent46" "UndergraduateStudent460" "UndergraduateStudent461" "UndergraduateStudent462" "UndergraduateStudent463" "UndergraduateStudent464" "UndergraduateStudent465" "UndergraduateStudent466" "UndergraduateStudent467" "UndergraduateStudent468" "UndergraduateStudent469" "UndergraduateStudent47" "UndergraduateStudent470" "UndergraduateStudent471" "UndergraduateStudent472" "UndergraduateStudent473" "UndergraduateStudent474" "UndergraduateStudent475" "UndergraduateStudent476" "UndergraduateStudent477" "UndergraduateStudent478" "UndergraduateStudent479" "UndergraduateStudent48" "UndergraduateStudent480" "UndergraduateStudent481" "UndergraduateStudent482" "UndergraduateStudent483" "UndergraduateStudent484" "UndergraduateStudent485" "UndergraduateStudent486" "UndergraduateStudent487" "UndergraduateStudent488" "UndergraduateStudent489" "UndergraduateStudent49" "UndergraduateStudent490" "UndergraduateStudent491" "UndergraduateStudent492" "UndergraduateStudent493" "UndergraduateStudent494" "UndergraduateStudent495" "UndergraduateStudent496" "UndergraduateStudent497" "UndergraduateStudent498" "UndergraduateStudent499" "UndergraduateStudent5" "UndergraduateStudent50" "UndergraduateStudent500" "UndergraduateStudent501" "UndergraduateStudent502" "UndergraduateStudent503" "UndergraduateStudent504" "UndergraduateStudent505" "UndergraduateStudent506" "UndergraduateStudent507" "UndergraduateStudent508" "UndergraduateStudent509" "UndergraduateStudent51" "UndergraduateStudent510" "UndergraduateStudent511" "UndergraduateStudent512" "UndergraduateStudent513" "UndergraduateStudent514" "UndergraduateStudent515" "UndergraduateStudent516" "UndergraduateStudent517" "UndergraduateStudent518" "UndergraduateStudent519" "UndergraduateStudent52" "UndergraduateStudent520" "UndergraduateStudent521" "UndergraduateStudent522" "UndergraduateStudent523" "UndergraduateStudent524" "UndergraduateStudent525" "UndergraduateStudent526" "UndergraduateStudent527" "UndergraduateStudent528" "UndergraduateStudent529" "UndergraduateStudent53" "UndergraduateStudent530" "UndergraduateStudent531" "UndergraduateStudent54" "UndergraduateStudent55" "UndergraduateStudent56" "UndergraduateStudent57" "UndergraduateStudent58" "UndergraduateStudent59" "UndergraduateStudent6" "UndergraduateStudent60" "UndergraduateStudent61" "UndergraduateStudent62" "UndergraduateStudent63" "UndergraduateStudent64" "UndergraduateStudent65" "UndergraduateStudent66" "UndergraduateStudent67" "UndergraduateStudent68" "UndergraduateStudent69" "UndergraduateStudent7" "UndergraduateStudent70" "UndergraduateStudent71" "UndergraduateStudent72" "UndergraduateStudent73" "UndergraduateStudent74" "UndergraduateStudent75" "UndergraduateStudent76" "UndergraduateStudent77" "UndergraduateStudent78" "UndergraduateStudent79" "UndergraduateStudent8" "UndergraduateStudent80" "UndergraduateStudent81" "UndergraduateStudent82" "UndergraduateStudent83" "UndergraduateStudent84" "UndergraduateStudent85" "UndergraduateStudent86" "UndergraduateStudent87" "UndergraduateStudent88" "UndergraduateStudent89" "UndergraduateStudent9" "UndergraduateStudent90" "UndergraduateStudent91" "UndergraduateStudent92" "UndergraduateStudent93" "UndergraduateStudent94" "UndergraduateStudent95" "UndergraduateStudent96" "UndergraduateStudent97" "UndergraduateStudent98" "UndergraduateStudent99" ) 

prop=$((32768 * $1 / 100)) 

for i in "${concepts[@]}"

do
		if [[ $RANDOM -le $prop ]]; then
			echo "domain(\"http://www.Department0.University0.edu/$i\")."
		fi
done


