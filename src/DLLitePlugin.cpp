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
 * @file DLLitePlugin.cpp
 * @author Daria Stepanova
 * @author Christoph Redl
 *
 * @brief Implements interface to DL-Lite using owlcpp.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "DLLitePlugin.h"
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

namespace dllite{

// ============================== Class CachedOntology ==============================

DLLitePlugin::CachedOntology::CachedOntology(RegistryPtr reg) : reg(reg){
	loaded = false;
	kernel = ReasoningKernelPtr(new ReasoningKernel());
}

DLLitePlugin::CachedOntology::~CachedOntology(){
}

void DLLitePlugin::CachedOntology::load(RegistryPtr reg, ID ontologyName){

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

		DBGLOG(DBG, "Done");
	}catch(std::exception e){
		throw PluginError("DLLite reasoner failed while loading file \"" + reg->terms.getByID(ontologyName).getUnquotedString() + "\", ensure that it is a valid ontology");
	}

	loaded = true;
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
				allIndividuals->setFact(addNamespaceToTerm(ogatom.tuple[i]).address);
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

bool DLLitePlugin::CachedOntology::checkConceptAssertion(RegistryPtr reg, ID guardAtomID) const{
	return conceptAssertions->getFact(guardAtomID.address);
}

bool DLLitePlugin::CachedOntology::checkRoleAssertion(RegistryPtr reg, ID guardAtomID) const{
	const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(guardAtomID.address);
	assert(ogatom.tuple.size() == 3 && "Role guard atoms must be of arity 2");
	BOOST_FOREACH (RoleAssertion ra, roleAssertions){
		if (ra.first == ogatom.tuple[0] && ra.second.first == ogatom.tuple[1] && ra.second.second == ogatom.tuple[2]) return true;
	}
	return false;
}

bool DLLitePlugin::CachedOntology::containsNamespace(std::string str) const{
	return str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace;
}

inline bool DLLitePlugin::CachedOntology::isOwlType(std::string str) const{
	return str.length() > 4 && str.substr(0, 4) == "owl:";
}

bool DLLitePlugin::CachedOntology::containsNamespace(ID term) const{
	return containsNamespace(reg->terms.getByID(term).getUnquotedString());
}

std::string DLLitePlugin::CachedOntology::addNamespaceToString(std::string str) const{
	DBGLOG(DBG, "Adding namespace to " + str);
	if (str[0] == '-') return "-" + ontologyNamespace + "#" + str.substr(1);
	else return ontologyNamespace + "#" + str;
}

std::string DLLitePlugin::CachedOntology::removeNamespaceFromString(std::string str) const{
	DBGLOG(DBG, "Removing namespace from " + str);
	assert((str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace) && "given string does not start with ontology namespace");
	if (str[0] == '-') return '-' + str.substr(ontologyNamespace.length() + 1 + 1); // +1 because of '#'
	return str.substr(ontologyNamespace.length() + 1); // +1 because of '#'
}

ID DLLitePlugin::CachedOntology::addNamespaceToTerm(ID term){
	return reg->storeConstantTerm("\"" + addNamespaceToString(reg->terms.getByID(term).getUnquotedString()) + "\"");
}

ID DLLitePlugin::CachedOntology::removeNamespaceFromTerm(ID term){
	return reg->storeConstantTerm("\"" + removeNamespaceFromString(reg->terms.getByID(term).getUnquotedString()) + "\"");
}

ID DLLitePlugin::CachedOntology::addNamespaceToAtom(ID atom){
	OrdinaryAtom oatom = reg->lookupOrdinaryAtom(atom);
	for (int i = 1; i < oatom.tuple.size(); ++i) oatom.tuple[i] = addNamespaceToTerm(oatom.tuple[i]);
	return reg->storeOrdinaryAtom(oatom);
}

ID DLLitePlugin::CachedOntology::removeNamespaceFromAtom(ID atom){
	OrdinaryAtom oatom = reg->lookupOrdinaryAtom(atom);
	for (int i = 0; i < oatom.tuple.size(); ++i)
		if (containsNamespace(reg->terms.getByID(oatom.tuple[i]).getUnquotedString()))
			oatom.tuple[i] = removeNamespaceFromTerm(oatom.tuple[i]);
	return reg->storeOrdinaryAtom(oatom);
}

InterpretationPtr DLLitePlugin::CachedOntology::addNamespaceToInterpretation(InterpretationPtr intr){

	InterpretationPtr newIntr = InterpretationPtr(new Interpretation(reg));
	bm::bvector<>::enumerator en = intr->getStorage().first();
	bm::bvector<>::enumerator en_end = intr->getStorage().end();
	while (en < en_end){
		newIntr->setFact(addNamespaceToAtom(reg->ogatoms.getIDByAddress(*en)).address);
		en++;
	}
	return newIntr;
}

InterpretationPtr DLLitePlugin::CachedOntology::removeNamespaceFromInterpretation(InterpretationPtr intr){

	InterpretationPtr newIntr = InterpretationPtr(new Interpretation(reg));
	bm::bvector<>::enumerator en = intr->getStorage().first();
	bm::bvector<>::enumerator en_end = intr->getStorage().end();
	while (en < en_end){
		newIntr->setFact(removeNamespaceFromAtom(reg->ogatoms.getIDByAddress(*en)).address);
		en++;
	}
	return newIntr;
}

// ============================== Class DLPluginAtom::Actor_collector ==============================

DLLitePlugin::DLPluginAtom::Actor_collector::Actor_collector(RegistryPtr reg, Answer& answer, CachedOntologyPtr ontology, Type t) : reg(reg), answer(answer), ontology(ontology), type(t){
	DBGLOG(DBG, "Instantiating Actor_collector");
}

DLLitePlugin::DLPluginAtom::Actor_collector::~Actor_collector(){
//	if (currentTuple.size() > 0) processTuple(currentTuple);
}

bool DLLitePlugin::DLPluginAtom::Actor_collector::apply(const TaxonomyVertex& node) {
	DBGLOG(DBG, "Actor collector called with " << node.getPrimer()->getName());
	ID tid = reg->storeConstantTerm("\"" + std::string(node.getPrimer()->getName()) + "\"");

	if (node.getPrimer()->getId() != -1 && !ontology->concepts->getFact(tid.address) && !ontology->roles->getFact(tid.address)){
		if (!ontology->containsNamespace(tid)){
			LOG(WARNING, "DLLite resoner returned constant " << RawPrinter::toString(reg, tid) << ", which seems to be not a valid individual name");
		}else{
			ID tidWithoutNamespace = reg->storeConstantTerm("\"" + ontology->removeNamespaceFromString(std::string(node.getPrimer()->getName())) + "\"");

			DBGLOG(DBG, "Adding element to tuple (ID=" << tidWithoutNamespace << ")");
			Tuple tup;
			tup.push_back(tidWithoutNamespace);
			answer.get().push_back(tup);
		}
	}

	return true;
}

// ============================== Class DLPluginAtom ==============================

DLLitePlugin::DLPluginAtom::DLPluginAtom(std::string predName, ProgramCtx& ctx) : PluginAtom(predName, true), ctx(ctx), learnedSupportSets(false){
}

ID DLLitePlugin::DLPluginAtom::dlNeg(ID id){

	RegistryPtr reg = getRegistry();
	if (reg->terms.getByID(id).getUnquotedString()[0] == '-') return reg->storeConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(1));
	else return reg->storeConstantTerm("\"-" + reg->terms.getByID(id).getUnquotedString() + "\"");
}

ID DLLitePlugin::DLPluginAtom::dlEx(ID id){

	RegistryPtr reg = getRegistry();
	return reg->storeConstantTerm("\"Ex" + reg->terms.getByID(id).getUnquotedString() + "\"");
}

std::string DLLitePlugin::DLPluginAtom::afterSymbol(std::string str, char c){

	if (str.find_last_of(c) == std::string::npos) return str;
	else return str.substr(str.find_last_of(c) + 1);
}

void DLLitePlugin::DLPluginAtom::constructClassificationProgram(){

	if (classificationIDB.size() > 0){
		DBGLOG(DBG, "Classification program was already constructed");
		return;
	}

	DBGLOG(DBG, "Constructing classification program");
	RegistryPtr reg = getRegistry();

	// prepare some terms and atoms
	subID = reg->storeConstantTerm("sub");
	opID = reg->storeConstantTerm("op");
	confID = reg->storeConstantTerm("conf");
	xID = reg->storeVariableTerm("X");
	yID = reg->storeVariableTerm("Y");
	zID = reg->storeVariableTerm("Z");
	ID x2ID = reg->storeVariableTerm("X2");
	ID y2ID = reg->storeVariableTerm("Y2");

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

	OrdinaryAtom confxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	confxy.tuple.push_back(confID);
	confxy.tuple.push_back(xID);
	confxy.tuple.push_back(yID);
	ID confxyID = reg->storeOrdinaryAtom(confxy);

	OrdinaryAtom opxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	opxy.tuple.push_back(opID);
	opxy.tuple.push_back(xID);
	opxy.tuple.push_back(yID);
	ID opxyID = reg->storeOrdinaryAtom(opxy);

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

	// Conflict rule: conf(X,Y) :- op(X,Y), sub(X,Y)
	Rule conflict(ID::MAINKIND_RULE);
	conflict.body.push_back(ID::posLiteralFromAtom(opxyID));
	conflict.body.push_back(ID::posLiteralFromAtom(subxyID));
	conflict.head.push_back(confxyID);
	ID conflictID = reg->storeRule(conflict);

	// assemble program
	classificationIDB.push_back(transID);
	classificationIDB.push_back(contraID);
	classificationIDB.push_back(conflictID);
}

void DLLitePlugin::DLPluginAtom::constructAbox(ProgramCtx& ctx, CachedOntologyPtr ontology){

	if (!!ontology->conceptAssertions){
		DBGLOG(DBG, "Skipping constructAbox (already done)");
	}

	DBGLOG(DBG, "Constructing Abox");
	RegistryPtr reg = getRegistry();
	ontology->conceptAssertions = InterpretationPtr(new Interpretation(reg));

	BOOST_FOREACH(owlcpp::Triple const& t, ontology->store.map_triple()) {
		DBGLOG(DBG, "Current triple: " << to_string(t.subj_, ontology->store) << " / " << to_string(t.pred_, ontology->store) << " / " << to_string(t.obj_, ontology->store));
		// concept assertion
		if (to_string(t.obj_, ontology->store) != "owl:Class" && !ontology->isOwlType(to_string(t.obj_, ontology->store)) && to_string(t.pred_, ontology->store) == "rdf:type") {
			ID conceptPredicate = reg->getAuxiliaryConstantSymbol('o', reg->storeConstantTerm("\"" + ontology->removeNamespaceFromString(to_string(t.obj_, ontology->store)) + "\""));
			OrdinaryAtom guard(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
			guard.tuple.push_back(conceptPredicate);
			guard.tuple.push_back(reg->storeConstantTerm("\"" + ontology->removeNamespaceFromString(to_string(t.subj_, ontology->store)) + "\""));
			ontology->conceptAssertions->setFact(reg->storeOrdinaryAtom(guard).address);
		}

		// role assertion
		if (ontology->containsNamespace(to_string(t.pred_, ontology->store))) {
			ontology->roleAssertions.push_back(
				CachedOntology::RoleAssertion(
		 			reg->storeConstantTerm("\"" + to_string(t.pred_, ontology->store) + "\""),
					std::pair<ID, ID>(
						reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""),
						reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\"") )));
		}

		// individual definition
		if (to_string(t.subj_, ontology->store) != "owl:Class" && to_string(t.obj_, ontology->store) == "owl:Thing" && to_string(t.pred_, ontology->store) == "rdf:type") {
			ontology->individuals->setFact(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"").address);
		}
	}
	DBGLOG(DBG, "Concept assertions: " << *ontology->conceptAssertions);
}

// computes the classification for a given ontology
InterpretationPtr DLLitePlugin::DLPluginAtom::computeClassification(ProgramCtx& ctx, CachedOntologyPtr ontology){

	assert(!ontology->classification && "Classification for this ontology was already computed");
	RegistryPtr reg = getRegistry();

	constructClassificationProgram();

	DBGLOG(DBG, "Computing classification");

	// prepare data structures for the subprogram P
	ProgramCtx pc = ctx;
	pc.idb = classificationIDB;
	InterpretationPtr edb = InterpretationPtr(new Interpretation(reg));
	pc.edb = edb;
	pc.currentOptimum.clear();
	InputProviderPtr ip(new InputProvider());
	pc.config.setOption("NumberOfModels",0);
	ip->addStringInput("", "empty");
	pc.inputProvider = ip;
	ip.reset();

	// use the ontology to construct the EDB
	ontology->concepts = InterpretationPtr(new Interpretation(reg));
	ontology->roles = InterpretationPtr(new Interpretation(reg));
	ontology->individuals = InterpretationPtr(new Interpretation(reg));
	DBGLOG(DBG,"Ontology file was loaded");
	BOOST_FOREACH(owlcpp::Triple const& t, ontology->store.map_triple()) {
		DBGLOG(DBG, "Current triple: " << to_string(t.subj_, ontology->store) << " / " << to_string(t.pred_, ontology->store) << " / " << to_string(t.obj_, ontology->store));
		if (afterSymbol(to_string(t.obj_, ontology->store), ':') == "Class" && afterSymbol(to_string(t.pred_, ontology->store), ':') == "type") {
			DBGLOG(DBG,"Construct facts of the form op(C,negC), sub(C,C) for this class.");
			ontology->concepts->setFact(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"").address);
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(opID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(dlNeg(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);;
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}	
		if (afterSymbol(to_string(t.obj_, ontology->store), ':') == "ObjectProperty" && afterSymbol(to_string(t.pred_, ontology->store), ':') == "type") {
			DBGLOG(DBG,"Construct facts of the form op(Subj,negSubj), sub(Subj,Subj), sub(exSubj,negexSubj), sub(exSubj,exSubj)");
			ontology->roles->setFact(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"").address);
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(opID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(dlNeg(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(dlEx(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				fact.tuple.push_back(dlEx(dlEx(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""))));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(dlEx(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				fact.tuple.push_back(dlEx(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}

		if (afterSymbol(to_string(t.pred_, ontology->store), ':') == "subClassOf")
		{
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\""));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}

		if (afterSymbol(to_string(t.pred_, ontology->store), ':') == "subPropertyOf")
		{
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\""));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}

		if (afterSymbol(to_string(t.pred_, ontology->store), ':') == "disjointWith")
		{
			DBGLOG(DBG,"Construct facts of the form sub(Subj,negObj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(dlNeg(reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\"")));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}
		if (afterSymbol(to_string(t.pred_, ontology->store), ':') == "propertyDisjointWith")
		{
			DBGLOG(DBG,"Construct facts of the form sub(Subj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\""));
				fact.tuple.push_back(dlNeg(reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\"")));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}
		if (afterSymbol(to_string(t.pred_, ontology->store), ':') == "Domain")
		{
			DBGLOG(DBG,"Construct facts of the form sub(exSubj,Obj)");
			{
				OrdinaryAtom fact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				fact.tuple.push_back(subID);
				fact.tuple.push_back(dlEx(reg->storeConstantTerm("\"" + to_string(t.subj_, ontology->store) + "\"")));
				fact.tuple.push_back(reg->storeConstantTerm("\"" + to_string(t.obj_, ontology->store) + "\""));
				edb->setFact(reg->storeOrdinaryAtom(fact).address);
			}
		}
	}
	DBGLOG(DBG, "EDB of classification program: " << *edb);

	// evaluate the subprogram and return its unique answer set
#ifndef NDEBUG
	InterpretationPtr intrWithoutNamespace = ontology->removeNamespaceFromInterpretation(pc.edb);
	DBGLOG(DBG, "LSS: Using the following facts as input to the classification program: " << *intrWithoutNamespace);
#endif
	std::vector<InterpretationPtr> answersets = ctx.evaluateSubprogram(pc, true);
	assert(answersets.size() == 1 && "Subprogram must have exactly one answer set");
	DBGLOG(DBG, "Classification: " << *answersets[0]);

	ontology->classification = answersets[0];
	assert(!!ontology->classification && "Could not compute classification");
	return ontology->classification;
}

DLLitePlugin::CachedOntologyPtr DLLitePlugin::DLPluginAtom::prepareOntology(ProgramCtx& ctx, ID ontologyNameID){

	std::vector<CachedOntologyPtr>& ontologies = ctx.getPluginData<DLLitePlugin>().ontologies;

	DBGLOG(DBG, "prepareOntology");
	RegistryPtr reg = getRegistry();

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
		co->load(reg, ontologyNameID);
		computeClassification(ctx, co);
		constructAbox(ctx, co);
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

void DLLitePlugin::DLPluginAtom::guardSupportSet(bool& keep, Nogood& ng, const ID eaReplacement)
{
	DBGLOG(DBG, "guardSupportSet");
	assert(ng.isGround());

	RegistryPtr reg = getRegistry();

	// get the ontology name
	ID ontologyNameID = reg->ogatoms.getByID(eaReplacement).tuple[1];
	CachedOntologyPtr ontology = prepareOntology(ctx, ontologyNameID);

	// find guard atom in the nogood
	BOOST_FOREACH (ID lit, ng){
		// since nogoods eliminate "unnecessary" property flags, we need to recover the original ID by retrieving it again
		ID litID = reg->ogatoms.getIDByAddress(lit.address);

		// check if it is a guard atom
		if (litID.isAuxiliary() && reg->getTypeByAuxiliaryConstantSymbol(litID) == 'o'){
			const OrdinaryAtom& guardAtom = reg->ogatoms.getByID(litID);

			// concept or role guard?
			bool holds;
			if (guardAtom.tuple.size() == 2){
				// concept guard
				holds = ontology->checkConceptAssertion(reg, litID);
			}else{
				assert(guardAtom.tuple.size() == 3 && "invalid guard atom");

				// role guard
				holds = ontology->checkRoleAssertion(reg, litID);
			}

			if (holds){
				// remove the guard atom
				Nogood restricted;
				BOOST_FOREACH (ID lit2, ng){
					if (lit2 != lit){
						restricted.insert(lit2);
					}
				}
				DBGLOG(DBG, "Keeping support set " << ng.getStringRepresentation(reg) << " with satisfied guard atom in form " << restricted.getStringRepresentation(reg));
				ng = restricted;
				keep = true;
			}else{
				DBGLOG(DBG, "Removing support set " << ng.getStringRepresentation(reg) << " because guard atom is unsatisfied");
				keep = false;
			}
		}
	}
	DBGLOG(DBG, "Keeping support set " << ng.getStringRepresentation(reg) << " without guard atom");
	keep = true;
}

void DLLitePlugin::DLPluginAtom::learnSupportSets(const Query& query, NogoodContainerPtr nogoods){

	DBGLOG(DBG, "LSS: Learning support sets");

	// make sure that the ontology is in the cache and retrieve its classification
	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);
	InterpretationPtr classification = ontology->classification;
#ifndef NDEBUG
	InterpretationPtr intrWithoutNamespace = ontology->removeNamespaceFromInterpretation(classification);
	DBGLOG(DBG, "LSS: Using the following model CM of the classification program: CM = " << *intrWithoutNamespace);
#endif
	RegistryPtr reg = getRegistry();

	// prepare output variable, tuple and negative output atom
	DBGLOG(DBG, "Storing output atom which will be part of any support set");
	ID outvarID = reg->storeVariableTerm("O");
	Tuple outlist;
	outlist.push_back(outvarID);
	ID outlit = NogoodContainer::createLiteral(ExternalLearningHelper::getOutputAtom(query, outlist, true)) | ID(ID::NAF_MASK, 0);
	DBGLOG(DBG, "LSS: Output atom is " << RawPrinter::toString(reg, ontology->removeNamespaceFromAtom(outlit)));

	// iterate over the maximum input
	DBGLOG(DBG, "LSS: Analyzing input to the external atom");
	bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
	bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();

	ID qID = ontology->addNamespaceToTerm(query.input[5]);

	#define DBGCHECKATOM(id) { std::stringstream ss; RawPrinter printer(ss, reg); printer.print(ontology->removeNamespaceFromAtom(id)); DBGLOG(DBG, "LSS:                Checking if " << ss.str() << " holds in CM:"); }
	while (en < en_end){
		// check if it is c+, c-, r+ or r-
		DBGLOG(DBG, "LSS:      Current input atom: " << RawPrinter::toString(reg, ontology->removeNamespaceFromAtom(reg->ogatoms.getIDByAddress(*en))));
		const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

		if (oatom.tuple[0] == query.input[1]){
			// c+
			DBGLOG(DBG, "LSS:           Atom belongs to c+");
			assert(oatom.tuple.size() == 3 && "Second parameter must be a binary predicate");

			ID cID = ontology->addNamespaceToTerm(oatom.tuple[1]);

			// check if sub(C, Q) is true in the classification assignment
			OrdinaryAtom subcq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subcq.tuple.push_back(subID);
			subcq.tuple.push_back(cID);
			subcq.tuple.push_back(qID);
			ID subcqID = reg->storeOrdinaryAtom(subcq);

			DBGCHECKATOM(subcqID)
			if (classification->getFact(subcqID.address)){
				OrdinaryAtom cpcx(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				cpcx.tuple.push_back(query.input[1]);
				cpcx.tuple.push_back(cID);
				cpcx.tuple.push_back(outvarID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcx)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS:                     Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS:                     Does not hold");
			}

			// check if conf(C, C) is true in the classification assignment
			OrdinaryAtom confcc(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			confcc.tuple.push_back(confID);
			confcc.tuple.push_back(cID);
			confcc.tuple.push_back(cID);
			ID confccID = reg->storeOrdinaryAtom(confcc);

			DBGCHECKATOM(confccID)
			if (classification->getFact(confccID.address)){
				OrdinaryAtom cpcx(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				cpcx.tuple.push_back(query.input[1]);
				cpcx.tuple.push_back(cID);
				cpcx.tuple.push_back(outvarID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcx)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS:                     Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS:                     Does not hold");
			}

			// check if sub(C, C') is true in the classification assignment (for some C')
			DBGLOG(DBG, "LSS:                Checking if sub(C, C') is true in the classification assignment (for some C, C')");
			bm::bvector<>::enumerator en2 = classification->getStorage().first();
			bm::bvector<>::enumerator en2_end = classification->getStorage().end();
			while (en2 < en2_end){
				DBGLOG(DBG, "Current classification atom: " << RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en2)));
				const OrdinaryAtom& cl = reg->ogatoms.getByAddress(*en2);
				if (cl.tuple[1] == cID){
					ID cWithoutNamespace = ontology->removeNamespaceFromTerm(cID);
#ifndef NDEBUG
					ID cpWithoutNamespace = ontology->removeNamespaceFromTerm(cl.tuple[2]);
					DBGLOG(DBG, "LSS:                     Found a match with C=" << RawPrinter::toString(reg, cWithoutNamespace) << " and C'=" << RawPrinter::toString(reg, cpWithoutNamespace));
#endif

					// add {c+(C, Y), negC'(Y)}
					Nogood supportset;

					OrdinaryAtom cpcy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
					cpcy.tuple.push_back(query.input[1]);
					cpcy.tuple.push_back(cWithoutNamespace);
					cpcy.tuple.push_back(yID);
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

					// guard atom
					OrdinaryAtom negcp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
					ID negcptID = dlNeg(cl.tuple[2]);
					ID aux = reg->getAuxiliaryConstantSymbol('o', negcptID);
					negcp.tuple.push_back(aux);
					negcp.tuple.push_back(yID);
					ID negcpID = reg->storeOrdinaryAtom(negcp);
					supportset.insert(NogoodContainer::createLiteral(negcpID));

					supportset.insert(outlit);

#ifndef NDEBUG
					std::string guardAtomStr = ontology->removeNamespaceFromString(reg->terms.getByID(negcptID).getUnquotedString());
					DBGLOG(DBG, "LSS:                     --> Learned support set: " << supportset.getStringRepresentation(reg) << " where " << RawPrinter::toString(reg, negcpID) << " is the guard atom " << guardAtomStr << "(" << RawPrinter::toString(reg, yID) << ")");
#endif

					nogoods->addNogood(supportset);

					// check if c-(C', Y) occurs in the maximal interpretation
					bm::bvector<>::enumerator en3 = query.interpretation->getStorage().first();
					bm::bvector<>::enumerator en3_end = query.interpretation->getStorage().end();
#ifndef NDEBUG
					std::string cpstr = RawPrinter::toString(reg, ontology->removeNamespaceFromTerm(cl.tuple[2]));
					DBGLOG(DBG, "LSS:                     Checking if (C',Y) with C'=" << cpstr << " occurs in c- (for some Y)");
#endif
					while (en3 < en3_end){
						const OrdinaryAtom& at = reg->ogatoms.getByAddress(*en3);
						if (at.tuple[0] == query.input[2] && at.tuple[1] == cl.tuple[2]){
							DBGLOG(DBG, "LSS:                          --> Found a match with Y=" << RawPrinter::toString(reg, at.tuple[2]));
							Nogood supportset;

							// add { T c+(C,Y), T c-(C,Y) }
							OrdinaryAtom cpcy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							cpcy.tuple.push_back(query.input[1]);
							cpcy.tuple.push_back(cID);
							cpcy.tuple.push_back(at.tuple[2]);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

							OrdinaryAtom cmcy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							cpcy.tuple.push_back(query.input[2]);
							cpcy.tuple.push_back(cID);
							cpcy.tuple.push_back(at.tuple[2]);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

							supportset.insert(outlit);

							DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
							nogoods->addNogood(supportset);
						}
						en3++;
					}
					DBGLOG(DBG, "LSS:                     Finished checking if (C',Y) with C'=" << cpstr << " occurs in c- (for some Y)");
				}
				en2++;
			}
			DBGLOG(DBG, "LSS:                Finished checking if sub(C, C') is true in the classification assignment (for some C, C')");
		}else if (oatom.tuple[0] == query.input[2]){
			// c-
			DBGLOG(DBG, "LSS:           Atom belongs to c-");
			assert(oatom.tuple.size() == 3 && "Third parameter must be a binary predicate");

			ID cID = oatom.tuple[1];

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subncq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subncq.tuple.push_back(subID);
			subncq.tuple.push_back(dlNeg(cID));
			subncq.tuple.push_back(qID);
			ID subncqID = reg->storeOrdinaryAtom(subncq);

			DBGCHECKATOM(subncqID)
			if (classification->getFact(subncqID.address)){
				OrdinaryAtom cmcx(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				cmcx.tuple.push_back(query.input[2]);
				cmcx.tuple.push_back(cID);
				cmcx.tuple.push_back(outvarID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cmcx)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS: Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS: Does not hold");
			}
		}else if (oatom.tuple[0] == query.input[3]){
			// r+
			DBGLOG(DBG, "LSS:           Atom belongs to r+");
			assert(oatom.tuple.size() == 4 && "Fourth parameter must be a ternary predicate");

			ID rID = oatom.tuple[1];

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subexrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subexrq.tuple.push_back(subID);
			subexrq.tuple.push_back(dlEx(rID));
			subexrq.tuple.push_back(qID);
			ID subexrqID = reg->storeOrdinaryAtom(subexrq);

			DBGCHECKATOM(subexrqID)
			if (classification->getFact(subexrqID.address)){
				OrdinaryAtom rprxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				rprxy.tuple.push_back(query.input[3]);
				rprxy.tuple.push_back(rID);
				rprxy.tuple.push_back(outvarID);
				rprxy.tuple.push_back(yID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS: Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS: Does not hold");
			}
		}else if (oatom.tuple[0] == query.input[4]){
			// r-
			DBGLOG(DBG, "LSS:           Atom belongs to r-");
			assert(oatom.tuple.size() == 4 && "Fifth parameter must be a ternary predicate");

			ID rID = oatom.tuple[1];

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subnexrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subnexrq.tuple.push_back(subID);
			subnexrq.tuple.push_back(dlNeg(dlEx(rID)));
			subnexrq.tuple.push_back(qID);
			ID subnexrqID = reg->storeOrdinaryAtom(subnexrq);

			DBGCHECKATOM(subnexrqID)
			if (classification->getFact(subnexrqID.address)){
				OrdinaryAtom rprxy(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
				rprxy.tuple.push_back(query.input[4]);
				rprxy.tuple.push_back(rID);
				rprxy.tuple.push_back(outvarID);
				rprxy.tuple.push_back(yID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS: Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS: Does not hold");
			}
		}
		en++;
	}
	DBGLOG(DBG, "LSS: Finished support set learning");
}

std::vector<TDLAxiom*> DLLitePlugin::DLPluginAtom::expandAbox(const Query& query){

	RegistryPtr reg = getRegistry();

	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);

	// add the additional assertions
	std::vector<TDLAxiom*> addedAxioms;

	DBGLOG(DBG, "Expanding Abox");
	bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
	bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();
	while (en < en_end){
		const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(*en);

		DBGLOG(DBG, "Checking " << RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en)));

		// determine type of additional assertion
		if (ogatom.tuple[0] == query.input[1] || ogatom.tuple[0] == query.input[2]){
			// c+ or c-
			assert(ogatom.tuple.size() == 3 && "Second parameter must be a binary predicate");
			ID concept = ontology->addNamespaceToTerm(ogatom.tuple[1]);
			if (!ontology->concepts->getFact(concept.address)){
				throw PluginError("Tried to expand concept \"" + RawPrinter::toString(reg, concept) + "\", which does not appear in the ontology");
			}
			ID individual = ogatom.tuple[2];
			DBGLOG(DBG, "Adding concept assertion: " << (ogatom.tuple[0] == query.input[2] ? "-" : "") << reg->terms.getByID(concept).getUnquotedString() << "(" << reg->terms.getByID(individual).getUnquotedString() << ")");
			TDLConceptExpression* factppConcept = ontology->kernel->getExpressionManager()->Concept(reg->terms.getByID(concept).getUnquotedString());
			if (ogatom.tuple[0] == query.input[2]) factppConcept = ontology->kernel->getExpressionManager()->Not(factppConcept);
			addedAxioms.push_back(ontology->kernel->instanceOf(
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual).getUnquotedString())),
					factppConcept));
		}else if (ogatom.tuple[0] == query.input[3] || ogatom.tuple[0] == query.input[4]){
			// r+ or r-
			assert(ogatom.tuple.size() == 4 && "Second parameter must be a ternery predicate");
			ID role = ontology->addNamespaceToTerm(ogatom.tuple[1]);
			if (!ontology->roles->getFact(role.address)){
				throw PluginError("Tried to expand role \"" + RawPrinter::toString(reg, role) + "\", which does not appear in the ontology");
			}
			ID individual1 = ogatom.tuple[2];
			ID individual2 = ogatom.tuple[3];
			DBGLOG(DBG, "Adding role assertion: " << (ogatom.tuple[0] == query.input[4] ? "-" : "") << reg->terms.getByID(role).getUnquotedString() << "(" << reg->terms.getByID(individual1).getUnquotedString() << ", " << reg->terms.getByID(individual2).getUnquotedString() << ")");
			TDLObjectRoleExpression* factppRole = ontology->kernel->getExpressionManager()->ObjectRole(reg->terms.getByID(role).getUnquotedString());

			if (ogatom.tuple[0] == query.input[4]){
				addedAxioms.push_back(ontology->kernel->relatedToNot(
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual1).getUnquotedString())),
					factppRole,
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual2).getUnquotedString()))));
			}else{
				addedAxioms.push_back(ontology->kernel->relatedTo(
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual1).getUnquotedString())),
					factppRole,
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual2).getUnquotedString()))));
			}
		}else{
			assert(false && "Invalid input atom");
		}

		en++;
	}
	return addedAxioms;
}

void DLLitePlugin::DLPluginAtom::restoreAbox(const Query& query, std::vector<TDLAxiom*> addedAxioms){

	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);

	// remove the axioms again
	BOOST_FOREACH (TDLAxiom* ax, addedAxioms){
		ontology->kernel->retract(ax);
	}
}

void DLLitePlugin::DLPluginAtom::retrieve(const Query& query, Answer& answer)
{
	assert(false && "this method should never be called since the learning-based method is present");
}

void DLLitePlugin::DLPluginAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods){

	DBGLOG(DBG, "DLPluginAtom::retrieve (" << !learnedSupportSets << ", " << !!nogoods << ", " << query.ctx->config.getOption("SupportSets") << ")");

	// check if we want to learn support sets (but do this only once)
	if (!learnedSupportSets && !!nogoods && query.ctx->config.getOption("SupportSets")){
		learnSupportSets(query, nogoods);
		learnedSupportSets = true;
	}
}

// ============================== Class CDLAtom ==============================

DLLitePlugin::CDLAtom::CDLAtom(ProgramCtx& ctx) : DLPluginAtom("cDL", ctx)
{
	DBGLOG(DBG,"Constructor of cDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputConstant(); // the query
	setOutputArity(1); // arity of the output list

	prop.supportSets = true; // we provide support sets
	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}

void DLLitePlugin::CDLAtom::retrieve(const Query& query, Answer& answer)
{
	assert(false);
}

void DLLitePlugin::CDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{
	DBGLOG(DBG, "CDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	// learn support sets (if enabled)
	DLPluginAtom::retrieve(query, answer, nogoods);

	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query);

	// handle inconsistency
	if (!ontology->kernel->isKBConsistent()){
		// add all individuals to the output
		DBGLOG(DBG, "KB is inconsistent: returning all tuples");
		InterpretationPtr intr = ontology->getAllIndividuals(query);
		bm::bvector<>::enumerator en = intr->getStorage().first();
		bm::bvector<>::enumerator en_end = intr->getStorage().end();
		while (en < en_end){
			Tuple tup;
			tup.push_back(ontology->removeNamespaceFromTerm(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en)));
			answer.get().push_back(tup);
			en++;
		}
		DBGLOG(DBG, "Query answering complete, recovering Abox");
		restoreAbox(query, addedAxioms);
		return;
	}

	// find the query concept
	DBGLOG(DBG, "Looking up query concept");
	bool found = false;
	std::string queryConcept = reg->terms.getByID(query.input[5]).getUnquotedString();
	bool negated = false;
	if (queryConcept[0] == '-'){
		negated = true;
		queryConcept = queryConcept.substr(1);
	}
	queryConcept = ontology->addNamespaceToString(queryConcept);
	BOOST_FOREACH(owlcpp::Triple const& t, ontology->store.map_triple()) {
		DBGLOG(DBG, "Current triple: " << to_string(t.subj_, ontology->store) << " / " << to_string(t.pred_, ontology->store) << " / " << to_string(t.obj_, ontology->store));
		if (to_string(t.subj_, ontology->store) == queryConcept){
			// found concept
			DBGLOG(DBG, "Preparing Actor_collector for " << to_string(t.subj_, ontology->store));
			Actor_collector ret(reg, answer, ontology, Actor_collector::Concept);
			DBGLOG(DBG, "Sending concept query");
			TDLConceptExpression* factppConcept = ontology->kernel->getExpressionManager()->Concept(to_string(t.subj_, ontology->store));
			if (negated) factppConcept = ontology->kernel->getExpressionManager()->Not(factppConcept);
			try{
				ontology->kernel->getInstances(factppConcept, ret);
			}catch(...){
				throw PluginError("DLLite reasoner failed during concept query");
			}
			found = true;
			break;
		}
	}
	if (!found) DBGLOG(WARNING, "Queried non-existing concept " << ontology->removeNamespaceFromString(queryConcept));

	DBGLOG(DBG, "Query answering complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class RDLAtom ==============================

DLLitePlugin::RDLAtom::RDLAtom(ProgramCtx& ctx) : DLPluginAtom("rDL", ctx)
{
	DBGLOG(DBG,"Constructor of rDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputConstant(); // the query
	setOutputArity(2); // arity of the output list

	prop.supportSets = true; // we provide support sets
	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}

void DLLitePlugin::RDLAtom::retrieve(const Query& query, Answer& answer)
{
	assert(false);
}

void DLLitePlugin::RDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "RDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	// learn support sets (if enabled)
	DLPluginAtom::retrieve(query, answer, nogoods);

	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query);

	// handle inconsistency
	if (!ontology->kernel->isKBConsistent()){
		// add all pairs of individuals to the output
		DBGLOG(DBG, "KB is inconsistent: returning all tuples");
		InterpretationPtr intr = ontology->getAllIndividuals(query);
		bm::bvector<>::enumerator en = intr->getStorage().first();
		bm::bvector<>::enumerator en_end = intr->getStorage().end();
		while (en < en_end){
			bm::bvector<>::enumerator en2 = intr->getStorage().first();
			bm::bvector<>::enumerator en2_end = intr->getStorage().end();
			while (en2 < en2_end){
				Tuple tup;
				tup.push_back(ontology->removeNamespaceFromTerm(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en)));
				tup.push_back(ontology->removeNamespaceFromTerm(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en2)));
				answer.get().push_back(tup);
				en2++;
			}
			en++;
		}
		DBGLOG(DBG, "Query answering complete, recovering Abox");
		restoreAbox(query, addedAxioms);
		return;
	}

	// get the query role
	std::string role = reg->terms.getByID(query.input[5]).getUnquotedString();
	bool negated = false;
	if (role[0] == '-'){
		role = role.substr(1);
		negated = true;
		// TODO: Are such queries possible in DLLite?
		// I did not find a FaCT++ method for constructing negative role expressions
		throw PluginError("Negative role queries are not supported");
	}
	role = ontology->addNamespaceToString(role);
	TDLObjectRoleExpression* factppRole = ontology->kernel->getExpressionManager()->ObjectRole(role);

	DBGLOG(DBG, "Answering role query");
	InterpretationPtr intr = ontology->getAllIndividuals(query);

	// for all individuals
	bm::bvector<>::enumerator en = intr->getStorage().first();
	bm::bvector<>::enumerator en_end = intr->getStorage().end();
	while (en < en_end){
		ID individual1 = ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en);
		ID individual1WithoutNamespace = ontology->removeNamespaceFromTerm(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));

		// query related individuals
		DBGLOG(DBG, "Querying individuals related to " << RawPrinter::toString(reg, individual1WithoutNamespace));
		std::vector<const TNamedEntry*> relatedIndividuals;
		try{
			ontology->kernel->getRoleFillers(
				ontology->kernel->getExpressionManager()->Individual(reg->terms.getByID(individual1).getUnquotedString()),
				factppRole,
				relatedIndividuals);
		}catch(...){
			throw PluginError("DLLite reasoner failed during role query");
		}

		// translate the result to HEX
		BOOST_FOREACH (const TNamedEntry* related, relatedIndividuals){
			std::string relatedIndividual = ontology->removeNamespaceFromString(related->getName());
			DBGLOG(DBG, "Adding role membership: (" << reg->terms.getByID(individual1WithoutNamespace).getUnquotedString() << ", " << relatedIndividual << ")");
			Tuple tup;
			tup.push_back(individual1WithoutNamespace);
			tup.push_back(reg->storeConstantTerm("\"" + relatedIndividual + "\""));
			answer.get().push_back(tup);
		}
		en++;
	}

	DBGLOG(DBG, "Query answering complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class RDLAtom ==============================

DLLitePlugin::ConsDLAtom::ConsDLAtom(ProgramCtx& ctx) : DLPluginAtom("consDL", ctx)
{
	DBGLOG(DBG,"Constructor of consDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	setOutputArity(0); // arity of the output list
}

void DLLitePlugin::ConsDLAtom::retrieve(const Query& query, Answer& answer)
{
	assert(false);
}

void DLLitePlugin::ConsDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "ConsDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	// learn support sets (if enabled)
	//DLPluginAtom::retrieve(query, answer, nogoods);

	CachedOntologyPtr ontology = prepareOntology(ctx, query.input[0]);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query);

	// handle inconsistency
	if (ontology->kernel->isKBConsistent()){
		answer.get().push_back(Tuple());
	}

	DBGLOG(DBG, "Consistency check complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class DLPlugin ==============================

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
	}

	for(std::vector<std::list<const char*>::iterator>::const_iterator it = found.begin(); it != found.end(); ++it){
		pluginOptions.erase(*it);
	}
}

void DLLitePlugin::printUsage(std::ostream& o) const{
	o << "     --repair=[ontology name]" << std::endl;
}

bool DLLitePlugin::providesCustomModelGeneratorFactory(ProgramCtx& ctx) const{
	return ctx.getPluginData<DLLitePlugin>().repair;
}

BaseModelGeneratorFactoryPtr DLLitePlugin::getCustomModelGeneratorFactory(ProgramCtx& ctx, const ComponentGraph::ComponentInfo& ci) const{
	return BaseModelGeneratorFactoryPtr(new RepairModelGeneratorFactory(ctx, ci));
}

dlvhex::dllite::DLLitePlugin theDLLitePlugin;
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
