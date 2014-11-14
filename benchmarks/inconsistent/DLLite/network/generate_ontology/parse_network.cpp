#include <boost/lambda/lambda.hpp>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <cmath> 
#include <utility>
#include <fstream>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp> 

int main()
{
	 
	std::ofstream file_network;
  	file_network.open ("network.owl",  std::fstream::in | std::fstream::out | std::fstream::app);
	std::ifstream input("transport.txt");
	std::vector<std::string> nodes;
	int number_of_edges = 0;
	int number_of_nodes = 0;
	std::string edge_endpoints1 = "endpoints1 = (";
	std::string edge_endpoints2 = "endpoints2 = (";

	
	for( std::string line; getline( input, line ); )
	{

		int p1 = line.find_first_of("(")+1;
		int p2 = line.find_first_of(",");

		std::string str1 = line.substr(p1, p2-p1);


		if (std::find(nodes.begin(), nodes.end(), str1) == nodes.end()){
			nodes.push_back(str1);
		}
		
		int p3 = p2+1;
		int p4 = line.find_first_of(".")-1;

		std::string str2 = line.substr(p3, p4-p3);


		if (std::find(nodes.begin(), nodes.end(), str2) == nodes.end()){
			nodes.push_back(str2);
		}
		
		std::stringstream ss_str_new1;
		int i = std::distance(nodes.begin(), find(nodes.begin(), nodes.end(), str1));
		ss_str_new1 <<'"'<<"#n"<<i<<'"';
		std::string str_new1 = ss_str_new1.str();


		std::stringstream ss_str_new2;
		int j = std::distance(nodes.begin(), find(nodes.begin(), nodes.end(), str2));
		ss_str_new2 <<'"'<<"#n"<<j<<'"';
		std::string str_new2 = ss_str_new2.str();

		file_network<<"<owl:Thing rdf:about="<<str_new1<<"><edge rdf:resource="<<str_new2<<"/></owl:Thing>"<<"\n";  

		std::stringstream ss_edge_endpoints1;
                ss_edge_endpoints1<<edge_endpoints1<<" "<<str_new1;
		edge_endpoints1 = ss_edge_endpoints1.str();
		

		std::stringstream ss_edge_endpoints2;
                ss_edge_endpoints2<<edge_endpoints2<<" "<<str_new2;
		edge_endpoints2 = ss_edge_endpoints2.str();
		

		number_of_edges++;			
	}
	number_of_nodes = nodes.size();
	std::stringstream ss_edge_endpoints1;
        ss_edge_endpoints1<<edge_endpoints1<<" )"<<std::endl;
	edge_endpoints1 = ss_edge_endpoints1.str();

	std::stringstream ss_edge_endpoints2;
        ss_edge_endpoints2<<edge_endpoints2<<" )"<<std::endl;
	edge_endpoints2 = ss_edge_endpoints2.str();

	
	std::cout<<"number of nodes: "<<number_of_nodes<<std::endl;

	std::cout<<"number of edges: "<<number_of_edges<<std::endl;

	std::cout<<edge_endpoints1<<"\n\n\n"<<std::endl;
	std::cout<<edge_endpoints2<<"\n\n\n"<<std::endl;

	file_network.close();
}

