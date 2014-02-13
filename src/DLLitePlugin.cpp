/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005, 2006, 2007 Roman Schindlauer
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Thomas Krennwallner
 * Copyright (C) 2009, 2010, 2011 Peter Schüller
 * Copyright (C) 2011, 2012, 2013, 2014 Christoph Redl
 * Copyright (C) 2014 Daria Stepanova
 * 
 * This file is part of dlvhex.
 *
 * dlvhex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * dlvhex is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlvhex; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/**
 * @file 	DLLitePlugin.cpp
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
 *
 * @brief Implements interface to DL-Lite using owlcpp.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "DLLitePlugin.h"
#include "ExternalAtoms.h"
#include "DLRewriter.h"
#include "RepairModelGenerator.h"
#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/ProgramCtx.h"
#include "dlvhex2/Registry.h"
#include "dlvhex2/Printer.h"
#include "dlvhex2/Printhelpers.h"
#include "dlvhex2/Logger.h"
#include "dlvhex2/ExternalLearningHelper.h"

#include <iostream>
#include <string>
#include <algorithm>

#include "boost/program_options.hpp"
#include "boost/range.hpp"
#include "boost/foreach.hpp"
#include "boost/filesystem.hpp"

#include "owlcpp/rdf/triple_store.hpp"
#include "owlcpp/rdf/query_triples.hpp"
#include "owlcpp/io/input.hpp"
#include "owlcpp/io/catalog.hpp"
#include "owlcpp/logic/triple_to_fact.hpp"
#include "owlcpp/terms/node_tags_owl.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

DLVHEX_NAMESPACE_BEGIN

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

#ifndef NDEBUG
	#define CheckPredefinedIDs ((theDLLitePlugin.subID != ID_FAIL && theDLLitePlugin.opID != ID_FAIL && theDLLitePlugin.confID != ID_FAIL && theDLLitePlugin.xID != ID_FAIL && theDLLitePlugin.yID != ID_FAIL && theDLLitePlugin.zID != ID_FAIL && theDLLitePlugin.guardPredicateID != ID_FAIL))
#endif

namespace dllite{
dlvhex::dllite::DLLitePlugin theDLLitePlugin;

// ============================== Class CachedOntology ==============================

DLLitePlugin::CachedOntology::CachedOntology(RegistryPtr reg) : reg(reg){
	loaded = false;
	kernel = ReasoningKernelPtr(new ReasoningKernel());
}

DLLitePlugin::CachedOntology::~CachedOntology(){
}

void DLLitePlugin::CachedOntology::load(ID ontologyName){

	assert(!loaded && "ontology was already loaded");
	assert(!!reg && "registry must be set before load is called");
	DBGLOG(DBG, "Assigning ontology name");
	this->ontologyName = ontologyName;

	// load and prepare the ontology here
	try{
		DBGLOG(DBG, "Reading file " << reg->terms.getByID(ontologyName).getUnquotedString());
		load_file(reg->terms.getByID(ontologyName).getUnquotedString(), store);

		DBGLOG(DBG, "Submitting ontology to reasoning kernel");
		submit(store, *kernel, true);

		DBGLOG(DBG, "Consistency of KB: " << kernel->isKBConsistent());

		DBGLOG(DBG, "Extracting ontology namespace");
		owlcpp::Catalog cat;
		add(cat, reg->terms.getByID(ontologyName).getUnquotedString(), false, 100);
		int oCount = 0;
		BOOST_FOREACH(const owlcpp::Doc_id id, cat){
			ontologyPath = cat.path(id);
			ontologyNamespace = cat.ontology_iri_str(id);
			ontologyVersion = cat.version_iri_str(id);
			oCount++;
		}
		DBGLOG(DBG, "Namespace is: " << ontologyNamespace << " (path: " << ontologyPath << ", version: " << ontologyVersion << ")");
		assert(oCount == 1 && "The file should contain exactly one ontology");
	}catch(...){
		throw PluginError("DLLite reasoner failed while loading file \"" + reg->terms.getByID(ontologyName).getUnquotedString() + "\", ensure that it is a valid ontology");
	}

	// now compute some meta-information
	analyzeTboxAndAbox();

	loaded = true;
}

void DLLitePlugin::CachedOntology::analyzeTboxAndAbox(){

	assert(!concepts && "analyzeTboxAndAbox must be called only once");
	assert(!!reg && "registry must be set before analyzeTboxAndAbox is called");
	assert(CheckPredefinedIDs && "IDs are not initialized");

	DBGLOG(DBG, "Analyzing ontology (Tbox and Abox)");
	concepts = InterpretationPtr(new Interpretation(reg));
	roles = InterpretationPtr(new Interpretation(reg));
	individuals = InterpretationPtr(new Interpretation(reg));
	conceptAssertions = InterpretationPtr(new Interpretation(reg));
	BOOST_FOREACH(owlcpp::Triple const& t, store.map_triple()) {
		std::string subj = to_string(t.subj_, store);
		std::string obj = to_string(t.obj_, store);
		std::string pred = to_string(t.pred_, store);

		DBGLOG(DBG, "Current triple: " << to_string(t.subj_, store) << " / " << pred << " / " << obj);

		// concept definition
		DBGLOG(DBG, "Checking if this is a concept definition");
		if (isOwlConstant(to_string(t.subj_, store)) && theDLLitePlugin.cmpOwlType(pred, "type") && theDLLitePlugin.cmpOwlType(obj, "Class")) {
			DBGLOG(DBG, "Yes");
			ID conceptID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(to_string(t.subj_, store)));
#ifndef NDEBUG
			std::string conceptStr = RawPrinter::toString(reg, conceptID);
			DBGLOG(DBG, "Found concept: " << conceptStr);
#endif
			concepts->setFact(conceptID.address);
		}else{
			DBGLOG(DBG, "No");
		}

		// role definition
		DBGLOG(DBG, "Checking if this is a role definition");
		if (isOwlConstant(to_string(t.subj_, store)) && theDLLitePlugin.cmpOwlType(pred, "type") && theDLLitePlugin.cmpOwlType(obj, "ObjectProperty")) {
			DBGLOG(DBG, "Yes");
			ID roleID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(to_string(t.subj_, store)));
#ifndef NDEBUG
			std::string roleStr = RawPrinter::toString(reg, roleID);
			DBGLOG(DBG, "Found role: " << roleStr);
#endif
			roles->setFact(roleID.address);
		}else{
			DBGLOG(DBG, "No");
		}

		// concept assertion
		DBGLOG(DBG, "Checking if this is a concept assertion");
		if (isOwlConstant(to_string(t.subj_, store)) && theDLLitePlugin.cmpOwlType(pred, "type") && isOwlConstant(obj)) {
			DBGLOG(DBG, "Yes");
			ID conceptID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj));
			ID individualID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(to_string(t.subj_, store)));
			OrdinaryAtom guard = theDLLitePlugin.getNewGuardAtom(true /* ground! */ );
			guard.tuple.push_back(conceptID);
			guard.tuple.push_back(individualID);
			ID guardAtomID = reg->storeOrdinaryAtom(guard);
			conceptAssertions->setFact(guardAtomID.address);
#ifndef NDEBUG
			std::string individualStr = RawPrinter::toString(reg, individualID);
			std::string conceptAssertionStr = theDLLitePlugin.printGuardAtom(guardAtomID);
			DBGLOG(DBG, "Found individual: " << individualStr);
			DBGLOG(DBG, "Found concept assertion: " << conceptAssertionStr);
#endif
			individuals->setFact(individualID.address);
		}else{
			DBGLOG(DBG, "No");
		}

		// role assertion
		DBGLOG(DBG, "Checking if this is a role assertion");
		if (isOwlConstant(pred) && isOwlConstant(pred) && isOwlConstant(obj)) {
			DBGLOG(DBG, "Yes");
			ID roleID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(pred));
			ID individual1ID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(to_string(t.subj_, store)));
			ID individual2ID = theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj));
#ifndef NDEBUG
			std::string roleAssertionStr = RawPrinter::toString(reg, roleID) + "(" + RawPrinter::toString(reg, individual1ID) + "," + RawPrinter::toString(reg, individual2ID) + ")";
			DBGLOG(DBG, "Found role assertion: " << roleAssertionStr);
#endif
			roleAssertions.push_back(
				RoleAssertion(
		 			roleID,
					std::pair<ID, ID>(
						individual1ID,
						individual2ID )));
		}else{
			DBGLOG(DBG, "No");
		}

		// individual definition
		DBGLOG(DBG, "Checking if this is an individual definition");
		if (isOwlConstant(to_string(t.subj_, store)) && theDLLitePlugin.cmpOwlType(obj, "Thing") && theDLLitePlugin.cmpOwlType(pred, "type")) {
			DBGLOG(DBG, "Yes");
			individuals->setFact(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(to_string(t.subj_, store))).address);
		}else{
			DBGLOG(DBG, "No");
		}
	}

	DBGLOG(DBG, "Concept assertions: " << *conceptAssertions);
}

#if 0
// This class is required if DLLitePlugin::CachedOntology::computeClassification computes the classification using FaCT++ (see below)
namespace{
class Actor_collector{
private:
	DLLitePlugin::CachedOntology& ontology;
	std::vector<std::string>& res;
public:
	
	Actor_collector(DLLitePlugin::CachedOntology& ontology, std::vector<std::string>& res) : ontology(ontology), res(res){
		DBGLOG(DBG, "Instantiating Actor_collector");
	}
	
	~Actor_collector(){
	}
	
	bool apply(const TaxonomyVertex& node) {
		DBGLOG(DBG, "Actor collector called with " << node.getPrimer()->getName());
		std::string returnValue(node.getPrimer()->getName());
	
		if (node.getPrimer()->getId() == -1 || !ontology.containsNamespace(returnValue)){
			DBGLOG(WARNING, "DLLite resoner returned constant " << returnValue << ", which seems to be not a valid individual name (will ignore it)");
		}else{
			res.push_back(returnValue);
		}
		return true;
	}
};
}
#endif

// computes the classification for a given ontology
void DLLitePlugin::CachedOntology::computeClassification(ProgramCtx& ctx){

	assert(!classification && "Classification for this ontology was already computed");

	DBGLOG(DBG, "Computing classification");

	#if 0
	// Alternatively to the computation of the classification using an ASP program,
	// it should also be possible to use FaCT++ as follows (but currently this does not work
	// because getSubConcepts delivers also individuals):
	classification = InterpretationPtr(new Interpretation(reg));
	{
		// for all concepts
		owlcpp::Triple_store::result_b<0,1,1,0>::type r = store.find_triple(
			owlcpp::any(),
			owlcpp::terms::rdf_type::id(),
			owlcpp::terms::owl_Class::id(),
			owlcpp::any());
		
		std::vector<std::string> res;
		BOOST_FOREACH(owlcpp::Triple const& t, r) {
			std::string subj = to_string(t.subj_, store);
			std::string obj = to_string(t.obj_, store);
			std::string pred = to_string(t.pred_, store);
			{
				Actor_collector col(*this, res);
				kernel->getSubConcepts(kernel->getExpressionManager()->Concept(subj), false, col);
				BOOST_FOREACH (std::string sub, res){
					OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					fact.tuple.push_back(theDLLitePlugin.subID);
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(sub)));
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
					classification->setFact(reg->storeOrdinaryAtom(fact).address);
				}
			}
			{
				Actor_collector col(*this, res);
				kernel-> getDisjointConcepts(kernel->getExpressionManager()->Concept(subj), col);
				BOOST_FOREACH (std::string sub, res){
					OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					fact.tuple.push_back(theDLLitePlugin.confID);
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(sub)));
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
					classification->setFact(reg->storeOrdinaryAtom(fact).address);
				}
			}
		}
	}
	
	{
		// for all roles
		owlcpp::Triple_store::result_b<0,1,1,0>::type r = store.find_triple(
			owlcpp::any(),
			owlcpp::terms::rdf_type::id(),
			owlcpp::terms::owl_ObjectProperty::id(),
			owlcpp::any());
		
		
		std::vector<std::string> res;
		BOOST_FOREACH(owlcpp::Triple const& t, r) {
			std::string subj = to_string(t.subj_, store);
			std::string obj = to_string(t.obj_, store);
			std::string pred = to_string(t.pred_, store);
			
			{
				Actor_collector col(*this, res);
				kernel->getSubRoles(kernel->getExpressionManager()->ObjectRole(subj), false, col);
				BOOST_FOREACH (std::string sub, res){
					OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					fact.tuple.push_back(theDLLitePlugin.subID);
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(sub));
					fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
					classification->setFact(reg->storeOrdinaryAtom(fact).address);
				}
			}
		}
	}
	DBGLOG(DBG, "Computed classification " << *classification);
	#endif

	// prepare data structures for the subprogram P
	InterpretationPtr edb = InterpretationPtr(new Interpretation(reg));

	// use the ontology to construct the EDB
	BOOST_FOREACH(owlcpp::Triple const& t, store.map_triple()) {
		std::string subj = to_string(t.subj_, store);
		std::string obj = to_string(t.obj_, store);
		std::string pred = to_string(t.pred_, store);

		DBGLOG(DBG, "Current triple: " << subj << " / " << pred << " / " << obj);
		DBGLOG(DBG, "Checking if this is a concept definition");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "type") && theDLLitePlugin.cmpOwlType(obj, "Class")) {
			DBGLOG(DBG, "Yes");

			// concepts should already be discovered in CachedOntology::analyzeTboxAndAbox
			assert(concepts->getFact(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)).address) && "found a concept which was previously missed");

			DBGLOG(DBG,"Construct facts of the form op(C,negC), sub(C,C) for this class.");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.opID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);;
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}
		DBGLOG(DBG, "Checking if this is a role definition");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "type") && theDLLitePlugin.cmpOwlType(obj, "ObjectProperty")) {
			DBGLOG(DBG, "Yes");

			// roles should already be discovered in CachedOntology::analyzeTboxAndAbox
			assert(roles->getFact(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)).address) && "found a role which was previously missed");

			DBGLOG(DBG,"Construct facts of the form op(Subj,negSubj), sub(Subj,Subj), op(exSubj,negexSubj), sub(exSubj,exSubj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.opID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.opID);
				fact.tuple.push_back(theDLLitePlugin.dlEx(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				fact.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.dlEx(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.dlEx(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				fact.tuple.push_back(theDLLitePlugin.dlEx(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is a concept inclusion");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "subclassOf") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is role inclusion");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "subpropertyOf") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is a concept disjointness axiom");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "disjointWith") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form sub(Subj,negObj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is a complement concept");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "complementOf") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form op(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.opID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is a role disjointness axiom");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "propertyDisjointWith") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj)));
				fact.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}

		DBGLOG(DBG, "Checking if this is a domain definition");
		if (isOwlConstant(subj) && theDLLitePlugin.cmpOwlType(pred, "domain") && isOwlConstant(obj))
		{
			DBGLOG(DBG, "Yes");
			DBGLOG(DBG,"Construct facts of the form sub(exSubj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(theDLLitePlugin.subID);
				fact.tuple.push_back(theDLLitePlugin.dlEx(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(subj))));
				fact.tuple.push_back(theDLLitePlugin.storeQuotedConstantTerm(removeNamespaceFromString(obj)));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}else{
			DBGLOG(DBG, "No");
		}
	}
	DBGLOG(DBG, "EDB of classification program: " << *edb);

	// evaluate the subprogram and return its unique answer set
#ifndef NDEBUG
	DBGLOG(DBG, "LSS: Using the following facts as input to the classification program: " << *edb);
#endif

	// evaluate the classification program without custom model generators

	ProgramCtx pc = ctx;
	pc.config.setOption("ForceGC", 0);
	pc.idb = theDLLitePlugin.classificationIDB;
	pc.edb = edb;
	pc.currentOptimum.clear();
	pc.customModelGeneratorProvider.reset();

	std::vector<InterpretationPtr> answersets = ctx.evaluateSubprogram(pc, false);
	assert(answersets.size() == 1 && "Subprogram must have exactly one answer set");
	DBGLOG(DBG, "Classification: " << *answersets[0]);

	classification = answersets[0];
	assert(!!classification && "Could not compute classification");
}

InterpretationPtr DLLitePlugin::CachedOntology::getAllIndividuals(const PluginAtom::Query& query){

	DBGLOG(DBG, "Retrieving all individuals");
	InterpretationPtr allIndividuals(new Interpretation(reg));

	// add individuals from the Abox
	allIndividuals->add(*individuals);

	// add individuals from the query
	{
		bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
		bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();
		while (en < en_end){
			const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(*en);
			assert((ogatom.tuple.size() == 3 || ogatom.tuple.size() == 4) && "invalid input atom");
			for (int i = 2; i < ogatom.tuple.size(); ++i){
				allIndividuals->setFact(ogatom.tuple[i].address);
			}
			en++;
		}
	}
#ifndef NDEBUG
	{
		std::stringstream ss;
		RawPrinter printer(ss, reg);
		bm::bvector<>::enumerator en = allIndividuals->getStorage().first();
		bm::bvector<>::enumerator en_end = allIndividuals->getStorage().end();
		while (en < en_end){
			printer.print(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));
			ss << " ";
			en++;
		}
		DBGLOG(DBG, "All individuals: " << ss.str());
	}
#endif
	return allIndividuals;
}

bool DLLitePlugin::CachedOntology::isOwlConstant(std::string str) const{
	// if it starts with the namespace, then it is definitely an owl constant
	if (str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace) return true;

	// otherwise, if it is not a type, then it is a constant
	return !theDLLitePlugin.isOwlType(str);
}

bool DLLitePlugin::CachedOntology::checkConceptAssertion(RegistryPtr reg, ID guardAtomID) const{
	assert(reg->ogatoms.getByAddress(guardAtomID.address).tuple.size() == 3 && "Concept guard atoms must be of arity 2");
	assert(!theDLLitePlugin.isDlEx(reg->ogatoms.getByID(guardAtomID).tuple[2]) && "existentials in guard atoms are disallowed");
	return conceptAssertions->getFact(guardAtomID.address);
}

bool DLLitePlugin::CachedOntology::checkRoleAssertion(RegistryPtr reg, ID guardAtomID) const{
	const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(guardAtomID.address);
	assert(ogatom.tuple.size() == 4 && "Role guard atoms must be of arity 2");
	assert(!theDLLitePlugin.isDlEx(ogatom.tuple[2]) && !theDLLitePlugin.isDlEx(ogatom.tuple[3]) && "existentials in guard atoms are disallowed");
	BOOST_FOREACH (RoleAssertion ra, roleAssertions){
		if (ra.first == ogatom.tuple[1] && ra.second.first == ogatom.tuple[2] && ra.second.second == ogatom.tuple[3]) return true;
	}
	return false;
}

// ============================== Class DLLitePlugin ==============================

void DLLitePlugin::constructClassificationProgram(ProgramCtx& ctx){

	assert(!!reg && "registry must be set before classification program can be constructed");

	assert (CheckPredefinedIDs && "IDs are not initialized");

	if (classificationIDB.size() > 0){
		DBGLOG(DBG, "Classification program was already constructed");
		return;
	}

	DBGLOG(DBG, "Constructing classification program");

	ID x2ID = reg->storeVariableTerm("X2");
	ID y2ID = reg->storeVariableTerm("Y2");
	ID y1ID = reg->storeVariableTerm("Y1");

	OrdinaryAtom subxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	subxy.tuple.push_back(subID);
	subxy.tuple.push_back(xID);
	subxy.tuple.push_back(yID);
	ID subxyID = reg->storeOrdinaryAtom(subxy);

	OrdinaryAtom subxz(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	subxz.tuple.push_back(subID);
	subxz.tuple.push_back(xID);
	subxz.tuple.push_back(zID);
	ID subxzID = reg->storeOrdinaryAtom(subxz);

	OrdinaryAtom subyz(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	subyz.tuple.push_back(subID);
	subyz.tuple.push_back(yID);
	subyz.tuple.push_back(zID);
	ID subyzID = reg->storeOrdinaryAtom(subyz);

	OrdinaryAtom opxx2(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opxx2.tuple.push_back(opID);
	opxx2.tuple.push_back(xID);
	opxx2.tuple.push_back(x2ID);
	ID opxx2ID = reg->storeOrdinaryAtom(opxx2);

	OrdinaryAtom opyy2(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opyy2.tuple.push_back(opID);
	opyy2.tuple.push_back(yID);
	opyy2.tuple.push_back(y2ID);
	ID opyy2ID = reg->storeOrdinaryAtom(opyy2);

	OrdinaryAtom suby2x2(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	suby2x2.tuple.push_back(subID);
	suby2x2.tuple.push_back(y2ID);
	suby2x2.tuple.push_back(x2ID);
	ID suby2x2ID = reg->storeOrdinaryAtom(suby2x2);

	OrdinaryAtom suby2y1(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	suby2y1.tuple.push_back(subID);
	suby2y1.tuple.push_back(y2ID);
	suby2y1.tuple.push_back(y1ID);
	ID suby2y1ID = reg->storeOrdinaryAtom(suby2y1);

	OrdinaryAtom confxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	confxy.tuple.push_back(confID);
	confxy.tuple.push_back(xID);
	confxy.tuple.push_back(yID);
	ID confxyID = reg->storeOrdinaryAtom(confxy);

	OrdinaryAtom confxy1(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	confxy1.tuple.push_back(confID);
	confxy1.tuple.push_back(xID);
	confxy1.tuple.push_back(y1ID);
	ID confxy1ID = reg->storeOrdinaryAtom(confxy1);

	OrdinaryAtom confxy2(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	confxy2.tuple.push_back(confID);
	confxy2.tuple.push_back(xID);
	confxy2.tuple.push_back(y2ID);
	ID confxy2ID = reg->storeOrdinaryAtom(confxy2);

	OrdinaryAtom opxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opxy.tuple.push_back(opID);
	opxy.tuple.push_back(xID);
	opxy.tuple.push_back(yID);
	ID opxyID = reg->storeOrdinaryAtom(opxy);

	OrdinaryAtom opyx(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opyx.tuple.push_back(opID);
	opyx.tuple.push_back(yID);
	opyx.tuple.push_back(xID);
	ID opyxID = reg->storeOrdinaryAtom(opyx);

	OrdinaryAtom opyy1(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opyy1.tuple.push_back(opID);
	opyy1.tuple.push_back(yID);
	opyy1.tuple.push_back(y1ID);
	ID opyy1ID = reg->storeOrdinaryAtom(opyy1);

	// Transitivity rule: sub(X,Z) :- sub(X,Y), sub(Y,Z)
	Rule trans(ID::MAINKIND_RULE);
	trans.body.push_back(ID::posLiteralFromAtom(subxyID));
	trans.body.push_back(ID::posLiteralFromAtom(subyzID));
	trans.head.push_back(subxzID);
	ID transID = reg->storeRule(trans);

	// Contraposition rule: sub(Y',X') :- op(X,X'), op(Y,Y'), sub(X,Y)
	Rule contra(ID::MAINKIND_RULE);
	contra.body.push_back(ID::posLiteralFromAtom(opxx2ID));
	contra.body.push_back(ID::posLiteralFromAtom(opyy2ID));
	contra.body.push_back(ID::posLiteralFromAtom(subxyID));
	contra.head.push_back(suby2x2ID);
	ID contraID = reg->storeRule(contra);

	// Conflict rule1: conf(X,Y1) :-  sub(X,Y), op(Y,Y1).
	Rule conflict(ID::MAINKIND_RULE);
	conflict.body.push_back(ID::posLiteralFromAtom(subxyID));	
	conflict.body.push_back(ID::posLiteralFromAtom(opyy1ID));
	conflict.head.push_back(confxy1ID);
	ID conflictID = reg->storeRule(conflict);

	// Conflict rule2: conf(X,Y2) :- conf(X,Y1), sub(Y2,Y1).
	Rule conflict2(ID::MAINKIND_RULE);
	conflict2.body.push_back(ID::posLiteralFromAtom(confxy1ID));	
	conflict2.body.push_back(ID::posLiteralFromAtom(suby2y1ID));
	conflict2.head.push_back(confxy2ID);
	ID conflict2ID = reg->storeRule(conflict2);

	// Rule for reflexivity of opposite predicate: op(Y,X) :- op(X,Y)
	Rule refop(ID::MAINKIND_RULE);
	refop.body.push_back(ID::posLiteralFromAtom(opxyID));
	refop.head.push_back(opyxID);
	ID refopID = reg->storeRule(refop);

	// assemble program
	classificationIDB.push_back(transID);
	classificationIDB.push_back(contraID);
	classificationIDB.push_back(conflictID);
	classificationIDB.push_back(conflict2ID);
	classificationIDB.push_back(refopID);

	DBGLOG(DBG, "Constructed classification program");
}

DLLitePlugin::CachedOntologyPtr DLLitePlugin::prepareOntology(ProgramCtx& ctx, ID ontologyNameID){

	std::vector<CachedOntologyPtr>& ontologies = ctx.getPluginData<DLLitePlugin>().ontologies;

	assert(!!reg && "Registry must be set for preparing ontologies");
	DBGLOG(DBG, "prepareOntology");

	BOOST_FOREACH (CachedOntologyPtr o, ontologies){
		if (o->ontologyName == ontologyNameID){
			DBGLOG(DBG, "Accessing cached ontology " << reg->terms.getByID(ontologyNameID).getUnquotedString());
			return o;
		}
	}

	// kill FaCT++ advertisement to stderr
	std::stringstream errstr;
	std::streambuf* origcerr = NULL;
	if( !Logger::Instance().shallPrint(Logger::DBG) )
	{
		// only do this if we are not debugging
		// (this origcerr procedure kills all logging in the following)
		origcerr = std::cerr.rdbuf(errstr.rdbuf());
	}


	// ontology is not in the cache --> load it
	DBGLOG(DBG, "Loading ontology" << reg->terms.getByID(ontologyNameID).getUnquotedString());

	CachedOntologyPtr co = CachedOntologyPtr(new CachedOntology(reg));
	try{
		co->load(ontologyNameID);
		ontologies.push_back(co);
	}catch(...){
		// restore stderr
		if(origcerr != NULL){
			std::cerr.rdbuf(origcerr);
			if(errstr.str().size() > 0) LOG(INFO, errstr.str());
		}
		throw;
	}

	// restore stderr
	if(origcerr != NULL){
		std::cerr.rdbuf(origcerr);
		if(errstr.str().size() > 0) LOG(INFO, errstr.str());
	}

	return co;
}

// Collect all types of external atoms 
DLLitePlugin::DLLitePlugin():
	PluginInterface()
{
	setNameVersion(PACKAGE_TARNAME,DLLITEPLUGIN_VERSION_MAJOR,DLLITEPLUGIN_VERSION_MINOR,DLLITEPLUGIN_VERSION_MICRO);
}

DLLitePlugin::~DLLitePlugin()
{
}

// Define two external atoms: for the roles and for the concept queries
std::vector<PluginAtomPtr> DLLitePlugin::createAtoms(ProgramCtx& ctx) const{
	std::vector<PluginAtomPtr> ret;
	ret.push_back(PluginAtomPtr(new CDLAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	ret.push_back(PluginAtomPtr(new RDLAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	ret.push_back(PluginAtomPtr(new ConsDLAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	ret.push_back(PluginAtomPtr(new InconsDLAtom(ctx), PluginPtrDeleter<PluginAtom>()));
	return ret;
}

void DLLitePlugin::processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx){

	std::vector<std::list<const char*>::iterator> found;
	for(std::list<const char*>::iterator it = pluginOptions.begin(); it != pluginOptions.end(); it++){
		std::string option(*it);
		if (option.find("--repair=") != std::string::npos){
			ctx.getPluginData<DLLitePlugin>().repair = true;
			ctx.getPluginData<DLLitePlugin>().repairOntology = option.substr(9);
			found.push_back(it);
		}
		if (option.find("--ontology=") != std::string::npos){
			ctx.getPluginData<DLLitePlugin>().rewrite = true;
			ctx.getPluginData<DLLitePlugin>().ontology = option.substr(11);
			found.push_back(it);
		}
		if (option == "--optimize"){
			ctx.getPluginData<DLLitePlugin>().optimize = true;
			found.push_back(it);
		}
	}

	for(std::vector<std::list<const char*>::iterator>::const_iterator it = found.begin(); it != found.end(); ++it){
		pluginOptions.erase(*it);
	}
}

// create parser modules that extend and the basic hex grammar
// this parser also stores the query information into the plugin
std::vector<HexParserModulePtr>
DLLitePlugin::createParserModules(ProgramCtx& ctx)
{
	DBGLOG(DBG,"DLLitePlugin::createParserModules(ProgramCtx& ctx)");
	if (!!this->reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == ctx.registry() && "DLLitePlugin: registry pointer passed in ctx.registry() to createParserModules(ProgramCtx& ctx) is different from previously set one, do not know what to do");
	}
	this->reg = ctx.registry();
	prepareIDs();

	std::vector<HexParserModulePtr> ret;

	DLLitePlugin::CtxData& ctxdata = ctx.getPluginData<DLLitePlugin>();
	if (ctxdata.rewrite){
		// the parser needs the registry, so make sure that the pointer is set
		DBGLOG(DBG,"rewriting is enabled");
		ID ontologyNameID = storeQuotedConstantTerm(ctxdata.ontology);
		DLRewriter::addParserModule(ctx, ret, prepareOntology(ctx, ontologyNameID));
	}

	return ret;
}

PluginRewriterPtr DLLitePlugin::createRewriter(ProgramCtx& ctx){

	DLLitePlugin::CtxData& ctxdata = ctx.getPluginData<DLLitePlugin>();
	if (ctxdata.optimize) return PluginRewriterPtr(new DLRewriter(ctxdata));
	else return PluginRewriterPtr();
}

void DLLitePlugin::printUsage(std::ostream& o) const{
	o << "     --repair=[ontology name]    Activates the repair model generator" << std::endl;
	o << "     --ontology=[ontology name]  Specifies the ontology used by DL-atoms" << std::endl;
	o << "     --optimize                  Rewrites default-negated consistency checking DL-atoms" << std::endl
	  << "                                 to inconsistency checks (makes them monotonic)" << std::endl;
}

void DLLitePlugin::setRegistry(RegistryPtr reg){

	DBGLOG(DBG,"DLLitePlugin::setRegistry(RegistryPtr reg)");
	if (!!this->reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == reg && "DLLitePlugin: registry pointer passed to setRegistry(RegistryPtr) is different from previously set one, do not know what to do");
	}
	this->reg = reg;
	prepareIDs();
}

void DLLitePlugin::setupProgramCtx(ProgramCtx& ctx){

	DBGLOG(DBG,"DLLitePlugin::setupProgramCtx(ProgramCtx& ctx)");
	if (!!reg){
		DBGLOG(DBG, "Registry was already previously set");
		assert(this->reg == ctx.registry() && "DLLitePlugin: registry pointer passed in ctx.registry() to setupProgramCtx(ProgramCtx& ctx) is different from previously set one, do not know what to do");
	}
	this->reg = ctx.registry();
	prepareIDs();
	constructClassificationProgram(ctx);
}

OrdinaryAtom DLLitePlugin::getNewAtom(ID pred, bool ground){
	OrdinaryAtom atom(ID::MAINKIND_ATOM);
	atom.kind |= (ground ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN);
	atom.kind |= (pred.isAuxiliary() ? ID::PROPERTY_AUX : 0);
	atom.tuple.push_back(pred);
	return atom;
}

OrdinaryAtom DLLitePlugin::getNewGuardAtom(bool ground){
	assert(CheckPredefinedIDs && "IDs are not initialized");
	OrdinaryAtom gatom(ID::MAINKIND_ATOM | ID::PROPERTY_GUARDAUX | ID::PROPERTY_AUX);
	gatom.kind |= (ground ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN);
	gatom.tuple.push_back(guardPredicateID);
	return gatom;
}

void DLLitePlugin::prepareIDs(){

	assert(!!reg && "registry must be set before IDs can be prepared");

#ifndef NDEBUG
	if (CheckPredefinedIDs){
		DBGLOG(DBG, "IDs have already been prepared");
		return;
	}
#endif

	// prepare some frequently used terms and atoms
	subID = reg->storeConstantTerm("sub");
	opID = reg->storeConstantTerm("op");
	confID = reg->storeConstantTerm("conf");
	xID = reg->storeVariableTerm("X");
	yID = reg->storeVariableTerm("Y");
	zID = reg->storeVariableTerm("Z");
	guardPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 0));
}


 
// Check whether custom model generator is enabled 
bool DLLitePlugin::providesCustomModelGeneratorFactory(ProgramCtx& ctx) const{
DBGLOG(DBG, "*** Before starting repair model generator");
	return ctx.getPluginData<DLLitePlugin>().repair;
}

// Create repair model generator
BaseModelGeneratorFactoryPtr DLLitePlugin::getCustomModelGeneratorFactory(ProgramCtx& ctx, const ComponentGraph::ComponentInfo& ci) const{
	return BaseModelGeneratorFactoryPtr(new RepairModelGeneratorFactory(ctx, ci));
}

}

DLVHEX_NAMESPACE_END

IMPLEMENT_PLUGINABIVERSIONFUNCTION

// return plain C type s.t. all compilers and linkers will like this code
extern "C"
void * PLUGINIMPORTFUNCTION()
{
	return reinterpret_cast<void*>(& dlvhex::dllite::theDLLitePlugin);
}

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
