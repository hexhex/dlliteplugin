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
 * @file 	ExternalAtoms.cpp
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
 *
 * @brief Implements the external atoms of the dllite plugin.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "DLLitePlugin.h"
#include "ExternalAtoms.h"
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

namespace dllite{
extern DLLitePlugin theDLLitePlugin;

// ============================== Class DLPluginAtom::Actor_collector ==============================

DLPluginAtom::Actor_collector::Actor_collector(RegistryPtr reg, Answer& answer, DLLitePlugin::CachedOntologyPtr ontology, Type t) : reg(reg), answer(answer), ontology(ontology), type(t){
	DBGLOG(DBG, "Instantiating Actor_collector");
}

DLPluginAtom::Actor_collector::~Actor_collector(){
}

bool DLPluginAtom::Actor_collector::apply(const TaxonomyVertex& node) {
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

DLPluginAtom::DLPluginAtom(std::string predName, ProgramCtx& ctx, bool monotonic) : PluginAtom(predName, monotonic), ctx(ctx){
}


bool DLPluginAtom::changeABox(const Query& query){

InterpretationConstPtr extintr = query.interpretation;
bm::bvector<>::enumerator enext = extintr->getStorage().first();
bm::bvector<>::enumerator enext_end = extintr->getStorage().end();
		while (enext < enext_end){
			const OrdinaryAtom& a = getRegistry()->ogatoms.getByAddress(*enext);
			if (a.tuple.size()==1)
				if ((a.tuple[0]==query.input[1])||(a.tuple[0]==query.input[2])||(a.tuple[0]==query.input[3])||(a.tuple[0]==query.input[4])) {
					return true;
				}
			enext++;
		}
return false;
}

void DLPluginAtom::guardSupportSet(bool& keep, Nogood& ng, const ID eaReplacement)
{   RegistryPtr reg = getRegistry();
	assert(ng.isGround());

	// get the ontology name
	const OrdinaryAtom& repl = reg->ogatoms.getByID(eaReplacement);
	ID ontologyNameID = repl.tuple[1];
	bool useAbox = (repl.tuple.size() == 6 || repl.tuple.size() == 7 && repl.tuple[6].address == 1);
	// use ABox is used for repair, it says whether the original ABox should be ignored or not
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, ontologyNameID, useAbox);

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

std::vector<TDLAxiom*> DLPluginAtom::expandAbox(const Query& query, bool useExistingAbox){

	RegistryPtr reg = getRegistry();

	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0], useExistingAbox);

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
			assert(((ogatom.tuple.size() == 3)||((ogatom.tuple.size() == 4)&&(ogatom.tuple[3].address<=1)&&(ogatom.tuple[3].isIntegerTerm()))) && "Second parameter must be a binary predicate");
			ID concept = ogatom.tuple[1];
			if (!ontology->concepts->getFact(concept.address)){
				throw PluginError("Tried to expand concept " + RawPrinter::toString(reg, concept) + ", which does not appear in the ontology");
			}
			ID individual = ogatom.tuple[2];
			DBGLOG(DBG, "Adding concept assertion: " << (ogatom.tuple[0] == query.input[2] ? "-" : "") << reg->terms.getByID(concept).getUnquotedString() << "(" << reg->terms.getByID(individual).getUnquotedString() << ")");
			TDLConceptExpression* factppConcept = ontology->kernel->getExpressionManager()->Concept(ontology->addNamespaceToString(reg->terms.getByID(concept).getUnquotedString()));
			if (ogatom.tuple.size()==4)
				if (ogatom.tuple[3].address==1) factppConcept = ontology->kernel->getExpressionManager()->Not(factppConcept);
 
			else if (ogatom.tuple[0] == query.input[2]) factppConcept = ontology->kernel->getExpressionManager()->Not(factppConcept);
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

void DLPluginAtom::restoreAbox(const Query& query, std::vector<TDLAxiom*> addedAxioms){

	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

	// remove the axioms again
	BOOST_FOREACH (TDLAxiom* ax, addedAxioms){
		ontology->kernel->retract(ax);
	}
}

void DLPluginAtom::retrieve(const Query& query, Answer& answer){
	assert(false && "this method should never be called since the learning-based method is present");
}

void DLPluginAtom::learnSupportSets(const Query& query, NogoodContainerPtr nogoods){

	DBGLOG(DBG, "LSS: Learning support sets");

	// make sure that the ontology is in the cache and retrieve its classification
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

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

CDLAtom::CDLAtom(ProgramCtx& ctx) : DLPluginAtom("cDL", ctx)
{
	DBGLOG(DBG,"Constructor of cDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputConstant(); // the query
	addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
	setOutputArity(1); // arity of the output list

	prop.supportSets = true; // we provide support sets
	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}



// called from the core
void CDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{


	DBGLOG(DBG, "CDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	if (query.input.size() > 7) throw PluginError("cDL accepts at most 7 parameters");
	if (query.input.size() == 7 && (!query.input[6].isIntegerTerm() || query.input[6].address >= 2)) throw PluginError("Last parameter of cDL must be 0 or 1");
	// TODO: add useAbox to other DL-atoms 
	bool useAbox = !changeABox(query)&&(query.input.size() < 7 || query.input[6].address == 1);
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0], useAbox);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

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

RDLAtom::RDLAtom(ProgramCtx& ctx) : DLPluginAtom("rDL", ctx)
{
	DBGLOG(DBG,"Constructor of rDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputConstant(); // the query
	addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
	setOutputArity(2); // arity of the output list

	prop.supportSets = true; // we provide support sets
	prop.completePositiveSupportSets = true; // we even provide (positive) complete support sets
}

void RDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "RDLAtom::retrieve");

	RegistryPtr reg = getRegistry();
	
	if (query.input.size() > 7) throw PluginError("rDL accepts at most 7 parameters");
	if (query.input.size() == 7 && (!query.input[6].isIntegerTerm() || query.input[6].address >= 2)) throw PluginError("Last parameter of rDL must be 0 or 1");
	bool useAbox = !changeABox(query)&&(query.input.size() < 7 || query.input[6].address == 1);
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0], useAbox);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

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

ConsDLAtom::ConsDLAtom(ProgramCtx& ctx) : DLPluginAtom("consDL", ctx, false)	// consistency check is NOT monotonic!
{
	DBGLOG(DBG,"Constructor of consDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
	setOutputArity(0); // arity of the output list
}

void ConsDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "ConsDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	if (query.input.size() > 6) throw PluginError("consDL accepts at most 6 parameters");
	if (query.input.size() == 6 && (!query.input[5].isIntegerTerm() || query.input[5].address >= 2)) throw PluginError("Last parameter of consDL must be 0 or 1");
	bool useAbox = !changeABox(query)&&(query.input.size() < 6 || query.input[5].address == 1);
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0], useAbox);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

	// handle inconsistency
	if (ontology->kernel->isKBConsistent()){
		answer.get().push_back(Tuple());
	}

	DBGLOG(DBG, "Consistency check complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

// ============================== Class InonsDLAtom ==============================

InconsDLAtom::InconsDLAtom(ProgramCtx& ctx) : DLPluginAtom("inconsDL", ctx)
{
	DBGLOG(DBG,"Constructor of inconsDL plugin is started");
	addInputConstant(); // the ontology
	addInputPredicate(); // the positive concept
	addInputPredicate(); // the negative concept
	addInputPredicate(); // the positive role
	addInputPredicate(); // the negative role
	addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
	setOutputArity(0); // arity of the output list
}

void InconsDLAtom::retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods)
{

	DBGLOG(DBG, "InconsDLAtom::retrieve");

	RegistryPtr reg = getRegistry();

	if (query.input.size() > 6) throw PluginError("inconsDL accepts at most 6 parameters");
	if (query.input.size() == 6 && (!query.input[5].isIntegerTerm() || query.input[5].address >= 2)) throw PluginError("Last parameter of inconsDL must be 0 or 1");

	bool useAbox = !changeABox(query)&&(query.input.size() < 6 || query.input[5].address == 1);
	DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0], useAbox);
	std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

	// handle inconsistency
	if (!ontology->kernel->isKBConsistent()){
		answer.get().push_back(Tuple());
	}

	DBGLOG(DBG, "Inconsistency check complete, recovering Abox");
	restoreAbox(query, addedAxioms);
}

}

DLVHEX_NAMESPACE_END

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
