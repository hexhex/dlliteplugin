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

// ============================== Class DLParserModuleSemantics ==============================
// (needs to be in dlvhex namespace)

#ifndef NDEBUG
	#define CheckPredefinedIDs ((theDLLitePlugin.subID != ID_FAIL && theDLLitePlugin.opID != ID_FAIL && theDLLitePlugin.confID != ID_FAIL && theDLLitePlugin.xID != ID_FAIL && theDLLitePlugin.yID != ID_FAIL && theDLLitePlugin.zID != ID_FAIL && theDLLitePlugin.guardPredicateID != ID_FAIL))
#endif

namespace dllite{
dlvhex::dllite::DLLitePlugin theDLLitePlugin;
}

class DLParserModuleSemantics:
	public HexGrammarSemantics
{
public:
	dllite::DLLitePlugin::CachedOntologyPtr ontology;
	dllite::DLLitePlugin::CtxData& ctxdata;

	DLParserModuleSemantics(ProgramCtx& ctx, dllite::DLLitePlugin::CachedOntologyPtr ontology):
		HexGrammarSemantics(ctx),
		ontology(ontology),
		ctxdata(ctx.getPluginData<dllite::DLLitePlugin>())
	{
	}

	struct dlAtom:
		SemanticActionBase<DLParserModuleSemantics, ID, dlAtom>
	{
		dlAtom(DLParserModuleSemantics& mgr):
			dlAtom::base_type(mgr)
		{
		}
	};

	struct dlExpression:
		SemanticActionBase<DLParserModuleSemantics, dllite::DLLitePlugin::DLExpression, dlExpression>
	{
		dlExpression(DLParserModuleSemantics& mgr):
			dlExpression::base_type(mgr)
		{
		}
	};
};

// create semantic handler for semantic action
template<>
struct sem<DLParserModuleSemantics::dlAtom>
{
	void operator()(
	DLParserModuleSemantics& mgr,
		const boost::fusion::vector3<
			const boost::optional<std::vector<dllite::DLLitePlugin::DLExpression> >,
			const boost::optional<std::string>,
		  	const boost::optional<std::vector<ID> >
		>& source,
	ID& target)
	{
		static int nextPred = 1;

		DBGLOG(DBG, "Parsing DL-atom with query " << boost::fusion::at_c<1>(source));
		RegistryPtr reg = mgr.ctx.registry();

		ID consDLID = reg->terms.getIDByString("consDL");
		ID cDLID = reg->terms.getIDByString("cDL");
		ID rDLID = reg->terms.getIDByString("rDL");

		std::vector<dllite::DLLitePlugin::DLExpression> emptyExpr;
		const std::vector<dllite::DLLitePlugin::DLExpression>& in = (!!boost::fusion::at_c<0>(source) ? boost::fusion::at_c<0>(source).get() : emptyExpr);

		// is there a query?
		ID query = ID_FAIL;
		if (!!boost::fusion::at_c<1>(source) ){
			query = reg->storeConstantTerm("\"" + boost::fusion::at_c<1>(source).get() + "\"");
		}

		bool haveQuery = (query != ID_FAIL);
		std::vector<ID> empty;
		const std::vector<ID>& out = haveQuery ? boost::fusion::at_c<2>(source).get() : empty;

		// check output arity for different types of queries
		if (query != ID_FAIL){
			if (mgr.ontology->concepts->getFact(query.address) && out.size() != 1) throw PluginError("Query for concept " + RawPrinter::toString(reg, query) + " must have one output variable");
			if (mgr.ontology->roles->getFact(query.address) && out.size() != 2) throw PluginError("Query for role " + RawPrinter::toString(reg, query) + " must have two output variables");
		}else{
			// there must be a vector of output variables iff there is a query
			if (out.size() != 0) throw PluginError("Consistency checks must not have output variables");
		}

		ExternalAtom ext(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_EXTERNAL);
		switch (out.size()){
			case 0:	ext.predicate = consDLID; break; // consistency check
			case 1: ext.predicate = cDLID; break; // concept query
			case 2: ext.predicate = rDLID; break; // role query
			default: throw PluginError("Invalid DL-atom");
		}

		// create predicates for c+, c-, r+ and r-
		ID cp = reg->getAuxiliaryConstantSymbol('o', ID(0, nextPred++));
		ID cm = reg->getAuxiliaryConstantSymbol('o', ID(0, nextPred++));
		ID rp = reg->getAuxiliaryConstantSymbol('o', ID(0, nextPred++));
		ID rm = reg->getAuxiliaryConstantSymbol('o', ID(0, nextPred++));

		// add rules for all DL-expressions related to this DL-atom
		ID varX = reg->storeVariableTerm("X");
		ID varY = reg->storeVariableTerm("Y");
		dllite::DLLitePlugin::CtxData& ctxdata = mgr.ctx.getPluginData<dllite::DLLitePlugin>();
		BOOST_FOREACH (dllite::DLLitePlugin::DLExpression dlexpression, in){
			Rule rule(ID::MAINKIND_RULE);

			ID conceptOrRoleID = reg->storeConstantTerm("\"" + dlexpression.conceptOrRole + "\"");

			// select appropriate auxiliary predicate
			OrdinaryAtom auxhead(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
			switch (dlexpression.type){
				case dllite::DLLitePlugin::DLExpression::plus: auxhead.tuple.push_back(mgr.ontology->concepts->getFact(conceptOrRoleID.address) ? cp : rp); break;
				case dllite::DLLitePlugin::DLExpression::minus: auxhead.tuple.push_back(mgr.ontology->concepts->getFact(conceptOrRoleID.address) ? cm : rm); break;
				default: assert(false);
			}

			// For concepts add a rule:	aux("C", X) :- pred(X)
			// For roles add a rule:	aux("R", X, Y) :- pred(X, Y)
			auxhead.tuple.push_back(conceptOrRoleID);
			auxhead.tuple.push_back(varX);
			if (!mgr.ontology->concepts->getFact(conceptOrRoleID.address)) auxhead.tuple.push_back(varY);
			rule.head.push_back(reg->storeOrdinaryAtom(auxhead));

			OrdinaryAtom bodyatom(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
			bodyatom.tuple.push_back(dlexpression.pred);
			bodyatom.tuple.push_back(varX);
			if (!mgr.ontology->concepts->getFact(conceptOrRoleID.address)) bodyatom.tuple.push_back(varY);
			rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(bodyatom)));

			// return ID of the aux predicate
			ID ruleID = reg->storeRule(rule);
			mgr.ctx.idb.push_back(ruleID);
		#ifndef NDEBUG
			std::string rulestr = RawPrinter::toString(reg, ruleID);
			DBGLOG(DBG, "Added DL-input rule: " << rulestr);
		#endif
		}

		// take output terms 1:1
		ext.inputs.push_back(mgr.ontology->ontologyName);
		ext.inputs.push_back(cp);
		ext.inputs.push_back(cm);
		ext.inputs.push_back(rp);
		ext.inputs.push_back(rm);
		if (haveQuery){
			ext.inputs.push_back(query);
		}
		ext.tuple = out;
		ID extID = reg->eatoms.storeAndGetID(ext);

		#ifndef NDEBUG
		std::string eatomstr = RawPrinter::toString(reg, extID);
		DBGLOG(DBG, "Created external atom " << eatomstr);
		#endif

		target = extID;
	}
};


// create semantic handler for semantic action
template<>
struct sem<DLParserModuleSemantics::dlExpression>
{
  void operator()(
    DLParserModuleSemantics& mgr,
		const boost::fusion::vector3<
			const std::string,
			const std::string,
		  	dlvhex::ID
		>& source,
    dllite::DLLitePlugin::DLExpression& target)
  {
	RegistryPtr reg = mgr.ctx.registry();

	DBGLOG(DBG, "Parsing DL-expression with concept or role " << boost::fusion::at_c<0>(source));
	dllite::DLLitePlugin::DLExpression expr;
	expr.conceptOrRole = boost::fusion::at_c<0>(source);
	std::string op = boost::fusion::at_c<1>(source);
	expr.pred = boost::fusion::at_c<2>(source);

	if (op.compare("+=") == 0){
		expr.type = dllite::DLLitePlugin::DLExpression::plus;
	}else if (op.compare("-=") == 0){
		expr.type = dllite::DLLitePlugin::DLExpression::minus;
	}else{
		throw PluginError("Unknown DL-atom expression: \"" + op + "\"");
	}


	target = expr;
  }
};

namespace dllite{

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

// computes the classification for a given ontology
void DLLitePlugin::CachedOntology::computeClassification(ProgramCtx& ctx){

	assert(!classification && "Classification for this ontology was already computed");

	DBGLOG(DBG, "Computing classification");

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
	std::vector<InterpretationPtr> answersets = ctx.evaluateSubprogram(edb, theDLLitePlugin.classificationIDB);
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

bool DLLitePlugin::CachedOntology::isOwlConstant(std::string str) const{
	// if it starts with the namespace, then it is definitely an owl constant
	if (str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace) return true;

	// otherwise, if it is not a type, then it is a constant
	return !theDLLitePlugin.isOwlType(str);
}

bool DLLitePlugin::CachedOntology::containsNamespace(std::string str) const{
	return (str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace);
}

std::string DLLitePlugin::CachedOntology::addNamespaceToString(std::string str) const{
	if (str[0] == '-') return "-" + ontologyNamespace + "#" + str.substr(1);
	else return ontologyNamespace + "#" + str;
}

std::string DLLitePlugin::CachedOntology::removeNamespaceFromString(std::string str) const{
	if (!(str.substr(0, ontologyNamespace.length()) == ontologyNamespace || (str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace))){
		DBGLOG(WARNING, "Constant \"" + str + "\" appears to be a constant of the ontology, but does not contain its namespace.");
		return str;
	}
	if (str[0] == '-') return '-' + str.substr(ontologyNamespace.length() + 1 + 1); // +1 because of '-', +1 because of '#'
	return str.substr(ontologyNamespace.length() + 1); // +1 because of '#'
}

// ============================== Class DLPluginAtom::Actor_collector ==============================

DLLitePlugin::DLPluginAtom::Actor_collector::Actor_collector(RegistryPtr reg, Answer& answer, CachedOntologyPtr ontology, Type t) : reg(reg), answer(answer), ontology(ontology), type(t){
	DBGLOG(DBG, "Instantiating Actor_collector");
}

DLLitePlugin::DLPluginAtom::Actor_collector::~Actor_collector(){
}

bool DLLitePlugin::DLPluginAtom::Actor_collector::apply(const TaxonomyVertex& node) {
	DBGLOG(DBG, "Actor collector called with " << node.getPrimer()->getName());
	std::string returnValue(node.getPrimer()->getName());

	if (node.getPrimer()->getId() == -1 || !ontology->containsNamespace(returnValue)){
		DBGLOG(WARNING, "DLLite resoner returned constant " << returnValue << ", which seems to be not a valid individual name (will ignore it)");
	}else{
		ID tid = theDLLitePlugin.storeQuotedConstantTerm(ontology->removeNamespaceFromString(returnValue));

		if (!ontology->concepts->getFact(tid.address) && !ontology->roles->getFact(tid.address)){
			DBGLOG(DBG, "Adding element to tuple (ID=" << tid << ")");
			Tuple tup;
			tup.push_back(tid);
			answer.get().push_back(tup);
		}
	}

	return true;
}

// ============================== Class DLPluginAtom ==============================

DLLitePlugin::DLPluginAtom::DLPluginAtom(std::string predName, ProgramCtx& ctx, bool monotonic) : PluginAtom(predName, monotonic), ctx(ctx){
}

void DLLitePlugin::DLPluginAtom::guardSupportSet(bool& keep, Nogood& ng, const ID eaReplacement)
{
	RegistryPtr reg = getRegistry();
	assert(ng.isGround());

	// get the ontology name
	ID ontologyNameID = reg->ogatoms.getByID(eaReplacement).tuple[1];
	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, ontologyNameID);

	DBGLOG(DBG, "Filtering SupportSet " << ng.getStringRepresentation(reg) << " wrt. " << *ontology->conceptAssertions);

	// find guard atom in the nogood
	BOOST_FOREACH (ID lit, ng){
		// since nogoods eliminate "unnecessary" property flags, we need to recover the original ID by retrieving it again
		ID litID = reg->ogatoms.getIDByAddress(lit.address);

		// check if it is a guard atom
		if (litID.isAuxiliary()){
			const OrdinaryAtom& possibleGuardAtom = reg->ogatoms.getByID(litID);
			if (possibleGuardAtom.tuple[0] != theDLLitePlugin.guardPredicateID) continue;

#ifndef NDEBUG
			std::string guardStr = theDLLitePlugin.printGuardAtom(litID);
			DBGLOG(DBG, "GUARD: Checking " << (possibleGuardAtom.tuple.size() == 2 ? "concept" : "role") << " guard atom " << guardStr);
#endif

			// concept or role guard?
			bool holds;
			if (possibleGuardAtom.tuple.size() == 3){
				// concept guard
				holds = ontology->checkConceptAssertion(reg, litID);
			}else{
				// role guard
				holds = ontology->checkRoleAssertion(reg, litID);
			}
			DBGLOG(DBG, "GUARD: Guard atom " << guardStr << " " << (holds ? " holds" : "does not hold"));

			if (holds){
				// remove the guard atom
				Nogood restricted;
				BOOST_FOREACH (ID lit2, ng){
					if (lit2 != lit){
						restricted.insert(lit2);
					}
				}
				DBGLOG(DBG, "GUARD: Keeping support set " << ng.getStringRepresentation(reg) << " with satisfied guard atom " << guardStr << " in form " << restricted.getStringRepresentation(reg));
				ng = restricted;
				keep = true;
				return;
			}else{
				DBGLOG(DBG, "GUARD: Removing support set " << ng.getStringRepresentation(reg) << " because guard atom " << guardStr << " is unsatisfied");
				keep = false;
				return;
			}
		}
	}
	DBGLOG(DBG, "GUARD: Keeping support set " << ng.getStringRepresentation(reg) << " without guard atom");
	keep = true;
}

std::vector<TDLAxiom*> DLLitePlugin::DLPluginAtom::expandAbox(const Query& query){

	RegistryPtr reg = getRegistry();

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

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
			ID concept = ogatom.tuple[1];
			if (!ontology->concepts->getFact(concept.address)){
				throw PluginError("Tried to expand concept " + RawPrinter::toString(reg, concept) + ", which does not appear in the ontology");
			}
			ID individual = ogatom.tuple[2];
			DBGLOG(DBG, "Adding concept assertion: " << (ogatom.tuple[0] == query.input[2] ? "-" : "") << reg->terms.getByID(concept).getUnquotedString() << "(" << reg->terms.getByID(individual).getUnquotedString() << ")");
			TDLConceptExpression* factppConcept = ontology->kernel->getExpressionManager()->Concept(ontology->addNamespaceToString(reg->terms.getByID(concept).getUnquotedString()));
			if (ogatom.tuple[0] == query.input[2]) factppConcept = ontology->kernel->getExpressionManager()->Not(factppConcept);
			addedAxioms.push_back(ontology->kernel->instanceOf(
					ontology->kernel->getExpressionManager()->Individual(ontology->addNamespaceToString(reg->terms.getByID(individual).getUnquotedString())),
					factppConcept));
		}else if (ogatom.tuple[0] == query.input[3] || ogatom.tuple[0] == query.input[4]){
			// r+ or r-
			assert(ogatom.tuple.size() == 4 && "Second parameter must be a ternery predicate");
			ID role = ogatom.tuple[1];
			if (!ontology->roles->getFact(role.address)){
				throw PluginError("Tried to expand role " + RawPrinter::toString(reg, role) + ", which does not appear in the ontology");
			}
			ID individual1 = ogatom.tuple[2];
			ID individual2 = ogatom.tuple[3];
			DBGLOG(DBG, "Adding role assertion: " << (ogatom.tuple[0] == query.input[4] ? "-" : "") << reg->terms.getByID(role).getUnquotedString() << "(" << reg->terms.getByID(individual1).getUnquotedString() << ", " << reg->terms.getByID(individual2).getUnquotedString() << ")");
			TDLObjectRoleExpression* factppRole = ontology->kernel->getExpressionManager()->ObjectRole(ontology->addNamespaceToString(reg->terms.getByID(role).getUnquotedString()));

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

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

	// remove the axioms again
	BOOST_FOREACH (TDLAxiom* ax, addedAxioms){
		ontology->kernel->retract(ax);
	}
}

void DLLitePlugin::DLPluginAtom::retrieve(const Query& query, Answer& answer){
	assert(false && "this method should never be called since the learning-based method is present");
}

void DLLitePlugin::DLPluginAtom::learnSupportSets(const Query& query, NogoodContainerPtr nogoods){

	DBGLOG(DBG, "LSS: Learning support sets");

	// make sure that the ontology is in the cache and retrieve its classification
	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

	// classify the Tbox if not already done
	if (!ontology->classification) ontology->computeClassification(ctx);
	InterpretationPtr classification = ontology->classification;

#ifndef NDEBUG
	DBGLOG(DBG, "LSS: Using the following model CM of the classification program: CM = " << *classification);
#endif
	RegistryPtr reg = getRegistry();

	// prepare output variable, tuple and negative output atom
	DBGLOG(DBG, "Storing output atom which will be part of any support set");
	ID outvarID = reg->storeVariableTerm("O");
	Tuple outlist;
	outlist.push_back(outvarID);
	ID outlit = NogoodContainer::createLiteral(ExternalLearningHelper::getOutputAtom(query, outlist, false));
#ifndef NDEBUG
	std::string outlitStr = RawPrinter::toString(reg, outlit);
	DBGLOG(DBG, "LSS: Output atom is " << outlitStr);
#endif

	// iterate over the maximum input
	DBGLOG(DBG, "LSS: Analyzing input to the external atom");
	bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
	bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();

	ID qID = query.input[5];

	#define DBGCHECKATOM(id) { std::stringstream ss; RawPrinter printer(ss, reg); printer.print(id); DBGLOG(DBG, "LSS:                Checking if " << ss.str() << " holds in CM:"); }
	while (en < en_end){
		// check if it is c+, c-, r+ or r-
#ifndef NDEBUG
		std::string enStr = RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en));
		DBGLOG(DBG, "LSS:      Current input atom: " << enStr);
#endif
		const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

		if (oatom.tuple[0] == query.input[1]){
			// c+
			DBGLOG(DBG, "LSS:           Atom belongs to c+");
			assert(oatom.tuple.size() == 3 && "Second parameter must be a binary predicate");

			ID cID = oatom.tuple[1];

			// check if sub(C, Q) is true in the classification assignment
			OrdinaryAtom subcq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subcq.tuple.push_back(theDLLitePlugin.subID);
			subcq.tuple.push_back(cID);
			subcq.tuple.push_back(qID);
			ID subcqID = reg->storeOrdinaryAtom(subcq);

			DBGCHECKATOM(subcqID)
			if (classification->getFact(subcqID.address)){
				OrdinaryAtom cpcx = theDLLitePlugin.getNewAtom(query.input[1]);
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
			confcc.tuple.push_back(theDLLitePlugin.confID);
			confcc.tuple.push_back(cID);
			confcc.tuple.push_back(cID);
			ID confccID = reg->storeOrdinaryAtom(confcc);

			DBGCHECKATOM(confccID)
			if (classification->getFact(confccID.address)){
				OrdinaryAtom cpcx = theDLLitePlugin.getNewAtom(query.input[1]);
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
#ifndef NDEBUG
			std::string cStr = RawPrinter::toString(reg, cID);

			DBGLOG(DBG, "LSS:                Checking if sub(" << cStr << ", C') is true in the classification assignment (for some C')");
#endif
			bm::bvector<>::enumerator en2 = classification->getStorage().first();
			bm::bvector<>::enumerator en2_end = classification->getStorage().end();
			while (en2 < en2_end){
#ifndef NDEBUG
				std::string en2Str = RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en2));
				DBGLOG(DBG, "LSS:                Current classification atom: " << en2Str);
#endif
				const OrdinaryAtom& clAtom = reg->ogatoms.getByAddress(*en2);
				if (clAtom.tuple[0] == theDLLitePlugin.subID && clAtom.tuple[1] == cID){
					ID cpID = clAtom.tuple[2];
#ifndef NDEBUG
					std::string cpStr = RawPrinter::toString(reg, cpID);
					DBGLOG(DBG, "LSS:                     Found a match with C=" << cStr << " and C'=" << cpStr);
#endif

					// add {c+(C, Y), negC'(Y)}
					Nogood supportset;

					OrdinaryAtom cpcy = theDLLitePlugin.getNewAtom(query.input[1]);
					cpcy.tuple.push_back(cID);
					cpcy.tuple.push_back(theDLLitePlugin.yID);
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

					// guard atom
					// since we cannot check guard atoms of form exR(Y), we rewrite them to R(Y,Z)
					ID guardID;
					if (theDLLitePlugin.isDlEx(cpID)){
						ID negnoexcpID = theDLLitePlugin.dlNeg(theDLLitePlugin.dlRemoveEx(clAtom.tuple[2]));
						OrdinaryAtom negnoexcp = theDLLitePlugin.getNewGuardAtom();
						negnoexcp.tuple.push_back(negnoexcpID);
						negnoexcp.tuple.push_back(theDLLitePlugin.yID);
						negnoexcp.tuple.push_back(theDLLitePlugin.zID);
						guardID = reg->storeOrdinaryAtom(negnoexcp);
						supportset.insert(NogoodContainer::createLiteral(guardID));
					}else{
						ID negcpID = theDLLitePlugin.dlNeg(clAtom.tuple[2]);
						OrdinaryAtom negcp = theDLLitePlugin.getNewGuardAtom();
						negcp.tuple.push_back(negcpID);
						negcp.tuple.push_back(theDLLitePlugin.yID);
						guardID = reg->storeOrdinaryAtom(negcp);
						supportset.insert(NogoodContainer::createLiteral(guardID));
					}

					supportset.insert(outlit);

#ifndef NDEBUG
					std::string negcpStr = RawPrinter::toString(reg, guardID);
					std::string guardStr = theDLLitePlugin.printGuardAtom(guardID);
					DBGLOG(DBG, "LSS:                     --> Learned support set: " << supportset.getStringRepresentation(reg) << " where " << negcpStr << " is the guard atom " << guardStr);
#endif

					nogoods->addNogood(supportset);

					// check if c-(C', Y) occurs in the maximal interpretation
					bm::bvector<>::enumerator en3 = query.interpretation->getStorage().first();
					bm::bvector<>::enumerator en3_end = query.interpretation->getStorage().end();
#ifndef NDEBUG
					DBGLOG(DBG, "LSS:                     Checking if (C',Y) with C'=" << cpStr << " occurs in c- (for some Y)");
#endif
					while (en3 < en3_end){
						const OrdinaryAtom& at = reg->ogatoms.getByAddress(*en3);
						if (at.tuple[0] == query.input[2] && at.tuple[1] == clAtom.tuple[2]){
#ifndef NDEBUG
							std::string yStr = RawPrinter::toString(reg, at.tuple[2]);
							DBGLOG(DBG, "LSS:                          --> Found a match with Y=" << yStr);
#endif
							Nogood supportset;

							// add { T c+(C,Y), T c-(C,Y) }
							OrdinaryAtom cpcy = theDLLitePlugin.getNewAtom(query.input[1], true);
							cpcy.tuple.push_back(cID);
							cpcy.tuple.push_back(at.tuple[2]);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

							OrdinaryAtom cmcy = theDLLitePlugin.getNewAtom(query.input[2], true);
							cmcy.tuple.push_back(cID);
							cmcy.tuple.push_back(at.tuple[2]);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cmcy)));

							supportset.insert(outlit);

							DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
							nogoods->addNogood(supportset);
						}
						en3++;
					}
					DBGLOG(DBG, "LSS:                     Finished checking if (C',Y) with C'=" << cpStr << " occurs in c- (for some Y)");
				}
				en2++;
			}
			DBGLOG(DBG, "LSS:                Finished checking if sub(" << cStr << ", C') is true in the classification assignment (for some C')");
		}else if (oatom.tuple[0] == query.input[2]){
			// c-
			DBGLOG(DBG, "LSS:           Atom belongs to c-");
			assert(oatom.tuple.size() == 3 && "Third parameter must be a binary predicate");

			ID cID = oatom.tuple[1];

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subncq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subncq.tuple.push_back(theDLLitePlugin.subID);
			subncq.tuple.push_back(theDLLitePlugin.dlNeg(cID));
			subncq.tuple.push_back(qID);
			ID subncqID = reg->storeOrdinaryAtom(subncq);

			DBGCHECKATOM(subncqID)
			if (classification->getFact(subncqID.address)){
				OrdinaryAtom cmcx = theDLLitePlugin.getNewAtom(query.input[2]);
				cmcx.tuple.push_back(cID);
				cmcx.tuple.push_back(outvarID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cmcx)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS:                     Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS:                     Does not hold");
			}
		}else if (oatom.tuple[0] == query.input[3]){
			// r+
			DBGLOG(DBG, "LSS:           Atom belongs to r+");
			assert(oatom.tuple.size() == 4 && "Fourth parameter must be a ternary predicate");

			ID rID = oatom.tuple[1];
			ID exrID = theDLLitePlugin.dlEx(rID);

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subexrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subexrq.tuple.push_back(theDLLitePlugin.subID);
			subexrq.tuple.push_back(theDLLitePlugin.dlEx(rID));
			subexrq.tuple.push_back(qID);
			ID subexrqID = reg->storeOrdinaryAtom(subexrq);

			DBGCHECKATOM(subexrqID)
			if (classification->getFact(subexrqID.address)){
				OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
				rprxy.tuple.push_back(rID);
				rprxy.tuple.push_back(outvarID);
				rprxy.tuple.push_back(theDLLitePlugin.yID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS:                     Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS:                     Does not hold");
			}
#ifndef NDEBUG
			std::string rIDStr = RawPrinter::toString(reg, rID);
			std::string exrIDStr = RawPrinter::toString(reg, exrID);

			DBGLOG(DBG, "LSS:                Checking if sub(" << exrIDStr << ", C) is true in the classification assignment (for some C)");
			DBGLOG(DBG, "LSS:                 or if if sub(" << rIDStr << ", R') is true in the classification assignment (for some R')");
#endif
			bm::bvector<>::enumerator en2 = classification->getStorage().first();
			bm::bvector<>::enumerator en2_end = classification->getStorage().end();
			while (en2 < en2_end){
				const OrdinaryAtom& at = reg->ogatoms.getByAddress(*en2);
				if (at.tuple[0] == query.input[3] && at.tuple[1] == exrID){
					DBGLOG(DBG, "LSS:                          --> Found a match with C=" << RawPrinter::toString(reg, at.tuple[2]));
					Nogood supportset;

					// add { T r+(R,O,Y), -C(O) }
					OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
					rprxy.tuple.push_back(rID);
					rprxy.tuple.push_back(outvarID);
					rprxy.tuple.push_back(theDLLitePlugin.yID);
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));

					// guard atom
					ID negctID = theDLLitePlugin.dlNeg(at.tuple[2]);
					OrdinaryAtom negc = theDLLitePlugin.getNewGuardAtom();
					negc.tuple.push_back(negctID);
					negc.tuple.push_back(outvarID);
					ID negcID = reg->storeOrdinaryAtom(negc);
					supportset.insert(NogoodContainer::createLiteral(negcID));

					supportset.insert(outlit);

					DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
					nogoods->addNogood(supportset);
				}
				if (at.tuple[0] == query.input[3] && at.tuple[1] == rID){
					DBGLOG(DBG, "LSS:                          --> Found a match with R'=" << RawPrinter::toString(reg, at.tuple[2]));
					Nogood supportset;

					// add { T r+(R,O,Y), -R'(O,Y) }
					OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
					rprxy.tuple.push_back(rID);
					rprxy.tuple.push_back(outvarID);
					rprxy.tuple.push_back(theDLLitePlugin.yID);
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));

					// guard atom
					ID negrptID = theDLLitePlugin.dlNeg(at.tuple[2]);
					OrdinaryAtom negrp = theDLLitePlugin.getNewGuardAtom();
					negrp.tuple.push_back(negrptID);
					negrp.tuple.push_back(outvarID);
					negrp.tuple.push_back(theDLLitePlugin.yID);
					ID negrpID = reg->storeOrdinaryAtom(negrp);
					supportset.insert(NogoodContainer::createLiteral(negrpID));

					supportset.insert(outlit);

					DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
					nogoods->addNogood(supportset);
				}

				en2++;
			}
		}else if (oatom.tuple[0] == query.input[4]){
			// r-
			DBGLOG(DBG, "LSS:           Atom belongs to r-");
			assert(oatom.tuple.size() == 4 && "Fifth parameter must be a ternary predicate");

			ID rID = oatom.tuple[1];

			// check if sub(negC, Q) is true in the classification assignment
			OrdinaryAtom subnexrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
			subnexrq.tuple.push_back(theDLLitePlugin.subID);
			subnexrq.tuple.push_back(theDLLitePlugin.dlNeg(theDLLitePlugin.dlEx(rID)));
			subnexrq.tuple.push_back(qID);
			ID subnexrqID = reg->storeOrdinaryAtom(subnexrq);

			DBGCHECKATOM(subnexrqID)
			if (classification->getFact(subnexrqID.address)){
				OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[4]);
				rprxy.tuple.push_back(rID);
				rprxy.tuple.push_back(outvarID);
				rprxy.tuple.push_back(theDLLitePlugin.yID);
				Nogood supportset;
				supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
				supportset.insert(outlit);
				DBGLOG(DBG, "LSS:                     Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
				nogoods->addNogood(supportset);
			}else{
				DBGLOG(DBG, "LSS:                     Does not hold");
			}
		}
		en++;
	}

	DBGLOG(DBG, "LSS: Analyzing query and Abox");
	{
		ID qID = query.input[5];

		// guard atom for Q(Y)
		OrdinaryAtom qy = theDLLitePlugin.getNewGuardAtom();
		qy.tuple.push_back(query.input[5]);
		qy.tuple.push_back(outvarID);
		Nogood supportset;
		supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(qy)));
		supportset.insert(outlit);
		DBGLOG(DBG, "LSS:      --> Learned support set: " << supportset.getStringRepresentation(reg));
		nogoods->addNogood(supportset);

		// check if sub(C, Q) is true in the classification assignment (for some C)
#ifndef NDEBUG
		std::string qstr = RawPrinter::toString(reg, query.input[5]);

		DBGLOG(DBG, "LSS:           Checking if sub(C, " << qstr << ") is true in the classification assignment (for some C')");
#endif
		bm::bvector<>::enumerator en = classification->getStorage().first();
		bm::bvector<>::enumerator en_end = classification->getStorage().end();
		while (en < en_end){
			DBGLOG(DBG, "LSS:                Current classification atom: " << RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en)));
			const OrdinaryAtom& clAtom = reg->ogatoms.getByAddress(*en);
			if (clAtom.tuple[0] == theDLLitePlugin.subID && clAtom.tuple[2] == qID){
				ID cID = clAtom.tuple[1];
#ifndef NDEBUG
				DBGLOG(DBG, "LSS:                     Found a match with C=" << RawPrinter::toString(reg, cID));
#endif
				if (theDLLitePlugin.isDlEx(clAtom.tuple[1])){
					DBGLOG(DBG, "LSS:                     (this is form exR)");
					// guard atom for C(O,Y)
					OrdinaryAtom roy = theDLLitePlugin.getNewGuardAtom();
					roy.tuple.push_back(theDLLitePlugin.dlRemoveEx(cID));
					roy.tuple.push_back(outvarID);
					roy.tuple.push_back(theDLLitePlugin.yID);
					Nogood supportset;
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(roy)));
					supportset.insert(outlit);
					DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
					nogoods->addNogood(supportset);
				}else{
					DBGLOG(DBG, "LSS:                     (this is not form exR)");
					// guard atom for C(O)
					OrdinaryAtom co = theDLLitePlugin.getNewGuardAtom();
					co.tuple.push_back(cID);
					co.tuple.push_back(outvarID);
					Nogood supportset;
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(co)));
					supportset.insert(outlit);
					DBGLOG(DBG, "LSS:                          --> Learned support set: " << supportset.getStringRepresentation(reg));
					nogoods->addNogood(supportset);
				}
			}
			en++;
		}
	}

	DBGLOG(DBG, "LSS: Finished support set learning");
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

void DLLitePlugin::CDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{
	DBGLOG(DBG, "CDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);
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
			tup.push_back(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));
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
	ID queryConceptID = query.input[5];
	bool negated = false;
	if (theDLLitePlugin.isDlNeg(query.input[5])){
		negated = true;
		queryConceptID = theDLLitePlugin.dlNeg(queryConceptID);
	}
	std::string queryConcept = ontology->addNamespaceToString(reg->terms.getByID(queryConceptID).getUnquotedString());
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
	if (!found){
		DBGLOG(WARNING, "Queried non-existing concept " << ontology->removeNamespaceFromString(queryConcept));
	}

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

void DLLitePlugin::RDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "RDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);
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
				tup.push_back(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));
				tup.push_back(ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en2));
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
	ID queryRoleID = query.input[5];
	bool negated = false;
	if (theDLLitePlugin.isDlNeg(queryRoleID)){
		queryRoleID = theDLLitePlugin.dlNeg(queryRoleID);
		negated = true;
		// TODO: Are such queries possible in DLLite?
		// I did not find a FaCT++ method for constructing negative role expressions
		throw PluginError("Negative role queries are not supported");
	}
	std::string role = reg->terms.getByID(queryRoleID).getUnquotedString();
	TDLObjectRoleExpression* factppRole = ontology->kernel->getExpressionManager()->ObjectRole(ontology->addNamespaceToString(role));

	DBGLOG(DBG, "Answering role query");
	InterpretationPtr intr = ontology->getAllIndividuals(query);

	// for all individuals
	bm::bvector<>::enumerator en = intr->getStorage().first();
	bm::bvector<>::enumerator en_end = intr->getStorage().end();
	while (en < en_end){
		ID individual = ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en);

		// query related individuals
		DBGLOG(DBG, "Querying individuals related to " << RawPrinter::toString(reg, individual));
		std::vector<const TNamedEntry*> relatedIndividuals;
		try{
			ontology->kernel->getRoleFillers(
				ontology->kernel->getExpressionManager()->Individual(
					ontology->addNamespaceToString(reg->terms.getByID(individual).getUnquotedString())),
					factppRole,
					relatedIndividuals);
		}catch(...){
			throw PluginError("DLLite reasoner failed during role query");
		}

		// translate the result to HEX
		BOOST_FOREACH (const TNamedEntry* related, relatedIndividuals){
			ID relatedIndividual = theDLLitePlugin.storeQuotedConstantTerm(ontology->removeNamespaceFromString(related->getName()));
			DBGLOG(DBG, "Adding role membership: (" << reg->terms.getByID(relatedIndividual).getUnquotedString() << ", " << relatedIndividual << ")");
			Tuple tup;
			tup.push_back(individual);
			tup.push_back(relatedIndividual);
			answer.get().push_back(tup);
		}
		en++;
	}

	DBGLOG(DBG, "Query answering complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class ConsDLAtom ==============================

DLLitePlugin::ConsDLAtom::ConsDLAtom(ProgramCtx& ctx) : DLPluginAtom("consDL", ctx, false)	// consistency check is NOT monotonic!
{
	DBGLOG(DBG,"Constructor of consDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	setOutputArity(0); // arity of the output list
}

void DLLitePlugin::ConsDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "ConsDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	// learn support sets (if enabled)
	//DLPluginAtom::retrieve(query, answer, nogoods);

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query);

	// handle inconsistency
	if (ontology->kernel->isKBConsistent()){
		answer.get().push_back(Tuple());
	}

	DBGLOG(DBG, "Consistency check complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class InonsDLAtom ==============================

DLLitePlugin::InconsDLAtom::InconsDLAtom(ProgramCtx& ctx) : DLPluginAtom("inconsDL", ctx)
{
	DBGLOG(DBG,"Constructor of inconsDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	setOutputArity(0); // arity of the output list
}

void DLLitePlugin::InconsDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "InconsDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	// learn support sets (if enabled)
	//DLPluginAtom::retrieve(query, answer, nogoods);

	CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query);

	// handle inconsistency
	if (!ontology->kernel->isKBConsistent()){
		answer.get().push_back(Tuple());
	}

	DBGLOG(DBG, "Inconsistency check complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

namespace
{

template<typename Iterator, typename Skipper>
struct DLParserModuleGrammarBase:
	// we derive from the original hex grammar
	// -> we can reuse its rules
	public HexGrammarBase<Iterator, Skipper>
{
	typedef HexGrammarBase<Iterator, Skipper> Base;

	template<typename Attrib=void, typename Dummy=void>
	struct Rule{
		typedef boost::spirit::qi::rule<Iterator, Attrib(), Skipper> type;
	};
	template<typename Dummy>
	struct Rule<void, Dummy>{
		typedef boost::spirit::qi::rule<Iterator, Skipper> type;
		// BEWARE: this is _not_ the same (!) as
		// typedef boost::spirit::qi::rule<Iterator, boost::spirit::unused_type, Skipper> type;
	};

	typename Rule<std::string>::type dlConceptOrRole;
	typename Rule<std::string>::type dlNegatedConceptOrRole;

	DLParserModuleSemantics& sem;

	qi::rule<Iterator, dllite::DLLitePlugin::DLExpression(), Skipper> dlExpression;
	qi::rule<Iterator, ID(), Skipper> dlAtom;

	DLParserModuleGrammarBase(DLParserModuleSemantics& sem):
		Base(sem),
		sem(sem)
	{
		typedef DLParserModuleSemantics Sem;

		dlConceptOrRole
		    = qi::lexeme[ ascii::alnum >> *(ascii::alnum) ];

		dlNegatedConceptOrRole
		    = qi::char_('-') >> dlConceptOrRole;

		dlExpression
			= (
					dlConceptOrRole >> qi::string("+=") >> Base::pred > qi::eps
				) [ Sem::dlExpression(sem) ]
			| (
					dlConceptOrRole >> qi::string("-=") >> Base::pred > qi::eps
				) [ Sem::dlExpression(sem) ];

		dlAtom
			= (
					qi::lit("DL") >> qi::lit('[') >> -(dlExpression % qi::lit(',')) >> qi::lit(';') >> -(dlConceptOrRole | dlNegatedConceptOrRole) >> qi::lit(']') >> qi::lit('(') >> -(Base::terms) >> qi::lit(')') > qi::eps
				) [ Sem::dlAtom(sem) ];

		#ifdef BOOST_SPIRIT_DEBUG
		BOOST_SPIRIT_DEBUG_NODE(dlAtom);
		BOOST_SPIRIT_DEBUG_NODE(dlExpression);
		#endif
	}
};

struct DLParserModuleGrammar:
  DLParserModuleGrammarBase<HexParserIterator, HexParserSkipper>,
	// required for interface
  // note: HexParserModuleGrammar =
	//       boost::spirit::qi::grammar<HexParserIterator, HexParserSkipper>
	HexParserModuleGrammar
{
	typedef DLParserModuleGrammarBase<HexParserIterator, HexParserSkipper> GrammarBase;
  typedef HexParserModuleGrammar QiBase;

  DLParserModuleGrammar(DLParserModuleSemantics& sem):
    GrammarBase(sem),
    QiBase(GrammarBase::dlAtom)
  {
  }
};
typedef boost::shared_ptr<DLParserModuleGrammar>
	DLParserModuleGrammarPtr;

// moduletype = HexParserModule::HEADATOM
template<enum HexParserModule::Type moduletype>
class DLParserModule:
	public HexParserModule
{
public:
	// the semantics manager is stored/owned by this module!
	DLParserModuleSemantics sem;
	// we also keep a shared ptr to the grammar module here
	DLParserModuleGrammarPtr grammarModule;

	DLParserModule(ProgramCtx& ctx, DLLitePlugin::CachedOntologyPtr ontology):
		HexParserModule(moduletype),
		sem(ctx, ontology)
	{
		LOG(INFO,"constructed DLParserModule");
	}

	virtual HexParserModuleGrammarPtr createGrammarModule()
	{
		assert(!grammarModule && "for simplicity (storing only one grammarModule pointer) we currently assume this will be called only once .. should be no problem to extend");
		grammarModule.reset(new DLParserModuleGrammar(sem));
		LOG(INFO,"created DLParserModuleGrammar");
		return grammarModule;
	}
};

} // anonymous namespace


class DLRewriter:
	public PluginRewriter
{
private:
	DLLitePlugin::CtxData& ctxdata;

public:
	DLRewriter(DLLitePlugin::CtxData& ctxdata) : ctxdata(ctxdata) {}
	virtual ~DLRewriter() {}

	virtual void rewrite(ProgramCtx& ctx){

		if (!ctxdata.optimize){
			DBGLOG(DBG, "Do not use DL-optimizer");
			return;
		}else{
			DBGLOG(DBG, "Using DL-optimizer");
		}

		RegistryPtr reg = ctx.registry();

		ID consDLID = reg->storeConstantTerm("consDL");
		ID inconsDLID = reg->storeConstantTerm("inconsDL");

		std::vector<ID> newIdb;
		std::vector<ID> newBody;
		bool ruleModified = false;
		bool programModified = false;
		BOOST_FOREACH (ID ruleID, ctx.idb){
#ifndef NDEBUG
			std::string rulestr;
			rulestr = RawPrinter::toString(reg, ruleID);
			DBGLOG(DBG, "Analyzing " << rulestr);
#endif
			const Rule& rule = reg->rules.getByID(ruleID);

			BOOST_FOREACH (ID batomID, rule.body){
#ifndef NDEBUG
				std::string litstr;
				litstr = RawPrinter::toString(reg, batomID);
				DBGLOG(DBG, "Analyzing " << litstr);
#endif
				// check if it is a default-negated consistency check
				if (batomID.isNaf() && batomID.isLiteral() && batomID.isExternalAtom()){
					const ExternalAtom& eatom = reg->eatoms.getByID(batomID);
					if (eatom.predicate == consDLID){
						DBGLOG(DBG, "Rewriting");
						// replace by inconsistency ID, store back and add _positively_ to the rule body
						ExternalAtom newEatom = eatom;
						newEatom.predicate = inconsDLID;
						ID newEatomID = reg->eatoms.storeAndGetID(newEatom);
						newBody.push_back(ID::posLiteralFromAtom(newEatomID));
						ruleModified = true;
#ifndef NDEBUG
						std::string newlitstr;
						newlitstr = RawPrinter::toString(reg, newEatomID);
						DBGLOG(DBG, "Rewrite " + newlitstr);
#endif
					}else{
						DBGLOG(DBG, "Do not rewrite");
						newBody.push_back(batomID);
					}
				}else{
					DBGLOG(DBG, "Do not rewrite");
					newBody.push_back(batomID);
				}
			}

			if (ruleModified){
				Rule newRule = rule;
				newRule.body = newBody;
				ID newRuleID = reg->storeRule(newRule);
				newIdb.push_back(newRuleID);
#ifndef NDEBUG
				std::string msg = "Optimized rule " + RawPrinter::toString(reg, ruleID) + " to " + RawPrinter::toString(reg, newRuleID);
				DBGLOG(DBG, msg);
#endif
				ruleModified = false;
				programModified = true;
			}else{
#ifndef NDEBUG
				bool equal = (newBody.size() == rule.body.size());
				for (int i = 0; i < newBody.size(); ++i){
					if (newBody[i] != rule.body[i]) equal = false;
				}
				assert (equal && "rule was modified by accident");
#endif
				newIdb.push_back(ruleID);
			}
			newBody.clear();
		}

		assert(ctx.idb.size() == newIdb.size() && "new program has a different number of rules");
#ifndef NDEBUG
		if (!programModified){
			for (int i = 0; i < newIdb.size(); ++i){
				assert(newIdb[i] == ctx.idb[i] && "program was modified by accident");
			}
		}
#endif
		ctx.idb = newIdb;
		DBGLOG(DBG, "Finished rewriting");
	}
};

// ============================== Class DLLitePlugin ==============================

ID DLLitePlugin::dlNeg(ID id){
	if (reg->terms.getByID(id).getUnquotedString()[0] == '-') return storeQuotedConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(1));
	else return storeQuotedConstantTerm("-" + reg->terms.getByID(id).getUnquotedString());
}

bool DLLitePlugin::isDlNeg(ID id){
	return (reg->terms.getByID(id).getUnquotedString()[0] == '-');
}

ID DLLitePlugin::dlEx(ID id){
	return storeQuotedConstantTerm("Ex:" + reg->terms.getByID(id).getUnquotedString());
}

ID DLLitePlugin::dlRemoveEx(ID id){
	assert(isDlEx(id) && "tried to translate exC to C, but given term is not of form exC");
	return storeQuotedConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(3));
}

ID DLLitePlugin::storeQuotedConstantTerm(std::string str){
#ifndef NDEBUG
	if (str[0] == '\"'){
		DBGLOG(WARNING, "Stored string " + str + ", which seems to contain duplicate quotation marks");
	}
	if (str.substr(0, 7).compare("http://") == 0 || str.substr(0, 8).compare("https://") == 0){
		DBGLOG(WARNING, "Stored string " + str + ", which seems to contain an absolute path including namespace; this should not happen");
	}
#endif
	return reg->storeConstantTerm("\"" + str + "\"");
}

bool DLLitePlugin::isOwlType(std::string str) const{

	// add prefixes to recognize here
	std::transform(str.begin(), str.end(), str.begin(), ::tolower);
	if (str.length() > 4 && str.substr(0, 4).compare("owl:") == 0) return true;
	if (str.length() > 4 && str.substr(0, 4).compare("rdf:") == 0) return true;
	if (str.length() > 5 && str.substr(0, 5).compare("rdfs:") == 0) return true;
	return false;
}

std::string DLLitePlugin::getOwlType(std::string str) const{

        if (str.find_last_of(':') == std::string::npos) return str;
        else return str.substr(str.find_last_of(':') + 1);

//	assert(isOwlType(str) && "tried to get the type of a string which does not start with owl:");
//	return str.substr(4);
}

bool DLLitePlugin::cmpOwlType(std::string str, std::string pattern) const{

	if (!isOwlType(str)) return false;
	std::string extracted = getOwlType(str);

	std::transform(extracted.begin(), extracted.end(), extracted.begin(), ::tolower);
	std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);

	return extracted == pattern;
}

bool DLLitePlugin::isDlEx(ID id){
	return (reg->terms.getByID(id).getUnquotedString().substr(0, 3).compare("Ex:") == 0);
}

std::string DLLitePlugin::printGuardAtom(ID atom){

	const OrdinaryAtom& oatom = reg->lookupOrdinaryAtom(atom);
	assert(ID(oatom.kind, 0).isGuardAuxiliary() && oatom.tuple[0] == guardPredicateID && "tried to print non-guard atom as guard atom");
	std::stringstream ss;
	ss << reg->terms.getByID(oatom.tuple[1]).getUnquotedString();
	ss << "(";
	ss << RawPrinter::toString(reg, oatom.tuple[2]);
	if (oatom.tuple.size() > 3) ss << ", " << RawPrinter::toString(reg, oatom.tuple[3]);
	ss << ")";
	return ss.str();
}

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
		ret.push_back(HexParserModulePtr(new DLParserModule<HexParserModule::BODYATOM>(ctx, prepareOntology(ctx, ontologyNameID))));
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
 
bool DLLitePlugin::providesCustomModelGeneratorFactory(ProgramCtx& ctx) const{
	return ctx.getPluginData<DLLitePlugin>().repair;
}

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
