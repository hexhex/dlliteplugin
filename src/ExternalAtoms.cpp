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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with dlvhex; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/**
 * @file ExternalAtoms.cpp
 * @author Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author Christoph Redl <redl@kr.tuwien.ac.at>
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

#include <cmath>
#include <utility>
#include <iostream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <stdio.h>
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

namespace dllite {
	extern DLLitePlugin theDLLitePlugin;

	// ============================== Class DLPluginAtom::Actor_collector ==============================

	DLPluginAtom::Actor_collector::Actor_collector(RegistryPtr reg, Answer& answer,
			DLLitePlugin::CachedOntologyPtr ontology, Type t) :
	reg(reg), answer(answer), ontology(ontology), type(t) {
		DBGLOG(DBG, "Instantiating Actor_collector");
	}

	DLPluginAtom::Actor_collector::~Actor_collector() {
	}

	bool DLPluginAtom::Actor_collector::apply(const TaxonomyVertex& node) {
		DBGLOG(DBG, "Actor collector called with " << node.getPrimer()->getName());
		std::string returnValue(node.getPrimer()->getName());

		if (node.getPrimer()->getId() == -1
				|| !ontology->containsNamespace(returnValue)) {
			DBGLOG(DBG,
					"DLLite reasoner returned constant " << returnValue << ", which seems to be not a valid individual name (will ignore it)");
		} else {
			ID tid = theDLLitePlugin.storeQuotedConstantTerm(
					ontology->removeNamespaceFromString(returnValue));

			if (!ontology->concepts->getFact(tid.address)
					&& !ontology->roles->getFact(tid.address)) {
				DBGLOG(DBG, "Adding element to tuple (ID=" << tid << ")");
				Tuple tup;
				tup.push_back(tid);
				answer.get().push_back(tup);
			}
		}

		return true;
	}

	// ============================== Class DLPluginAtom ==============================

	DLPluginAtom::DLPluginAtom(std::string predName, ProgramCtx& ctx,
			bool monotonic) :
	PluginAtom(predName, monotonic), ctx(ctx) {
	}

	bool DLPluginAtom::changeABox(const Query& query) {

		InterpretationConstPtr extintr = query.interpretation;
		bm::bvector<>::enumerator enext = extintr->getStorage().first();
		bm::bvector<>::enumerator enext_end = extintr->getStorage().end();
		while (enext < enext_end) {
			const OrdinaryAtom& a = getRegistry()->ogatoms.getByAddress(*enext);
			if (a.tuple.size() == 1)
			if ((a.tuple[0] == query.input[1]) || (a.tuple[0] == query.input[2])
					|| (a.tuple[0] == query.input[3])
					|| (a.tuple[0] == query.input[4])) {
				DBGLOG(DBG,"ignore the original ABox");

				return true;
			}
			enext++;
		}
		return false;
	}

	void DLPluginAtom::guardSupportSet(bool& keep, Nogood& ng,
			const ID eaReplacement) {
		RegistryPtr reg = getRegistry();
		assert(ng.isGround());

		// get the ontology name
		const OrdinaryAtom& repl = reg->ogatoms.getByID(eaReplacement);
		ID ontologyNameID = repl.tuple[1];
		bool useAbox = (repl.tuple.size() == 6
				|| repl.tuple.size() == 7 && repl.tuple[6].address == 1);
		// use ABox is used for repair, it says whether the original ABox should be ignored or not
		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, ontologyNameID, useAbox);

		DBGLOG(DBG,
				"Filtering SupportSet " << ng.getStringRepresentation(reg) << " wrt. " << *ontology->conceptAssertions);

		// find guard atom in the nogood
		BOOST_FOREACH (ID lit, ng) {
			// since nogoods eliminate "unnecessary" property flags, we need to recover the original ID by retrieving it again
			ID litID = reg->ogatoms.getIDByAddress(lit.address);

			// check if it is a guard atom
			if (litID.isAuxiliary()) {
				const OrdinaryAtom& possibleGuardAtom = reg->ogatoms.getByID(litID);
				if (possibleGuardAtom.tuple[0] != theDLLitePlugin.guardPredicateID)
				continue;

#ifndef NDEBUG
				std::string guardStr = theDLLitePlugin.printGuardAtom(litID);

				DBGLOG(DBG,
						"GUARD: Checking " << (possibleGuardAtom.tuple.size() == 3 ? "concept" : "role") << " guard atom " << guardStr);
#endif

				// concept or role guard?
				bool holds;
				if (possibleGuardAtom.tuple.size() == 3) {
					// concept guard
					holds = ontology->checkConceptAssertion(reg, litID);
				} else {
					// role guard
					holds = ontology->checkRoleAssertion(reg, litID);
				}
				DBGLOG(DBG,
						"GUARD: Guard atom " << guardStr << " " << (holds ? " holds" : "does not hold"));

				if (holds) {
					// remove the guard atom
					Nogood restricted;
					BOOST_FOREACH (ID lit2, ng) {
						if (lit2 != lit) {
							restricted.insert(lit2);
						}
					}
					DBGLOG(DBG,
							"GUARD: Keeping support set " << ng.getStringRepresentation(reg) << " with satisfied guard atom " << guardStr << " in form " << restricted.getStringRepresentation(reg));
					ng = restricted;
					keep = true;
					return;
				} else {
					DBGLOG(DBG,
							"GUARD: Removing support set " << ng.getStringRepresentation(reg) << " because guard atom " << guardStr << " is unsatisfied");
					keep = false;
					return;
				}
			}
		}
		DBGLOG(DBG,
				"GUARD: Keeping support set " << ng.getStringRepresentation(reg) << " without guard atom");
		keep = true;
	}

	std::vector<TDLAxiom*> DLPluginAtom::expandAbox(const Query& query,
			bool useExistingAbox) {
		DBGLOG(DBG,"Expand Abox is started with useAbox = "<<useExistingAbox);
		RegistryPtr reg = getRegistry();

		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0], useExistingAbox);

		// add the additional assertions
		std::vector<TDLAxiom*> addedAxioms;

		DBGLOG(DBG, "Expanding Abox");
		bm::bvector<>::enumerator en = query.interpretation->getStorage().first();
		bm::bvector<>::enumerator en_end = query.interpretation->getStorage().end();
		while (en < en_end) {
			const OrdinaryAtom& ogatom = reg->ogatoms.getByAddress(*en);

			DBGLOG(DBG,
					"Checking " << RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en)));

			// determine type of additional assertion
			if (ogatom.tuple[0] == query.input[1]
					|| ogatom.tuple[0] == query.input[2]) {
				if (useExistingAbox||(ogatom.tuple.size() == 3)) {
				// c+ or c-
				assert(
						((ogatom.tuple.size() == 3)
								|| ((ogatom.tuple.size() == 4)
										&& (ogatom.tuple[3].address <= 1)
										&& (ogatom.tuple[3].isIntegerTerm())))
						&& "Second parameter must be a binary predicate");
				ID concept = ogatom.tuple[1];
				ID newauxIDun = theDLLitePlugin.storeQuotedConstantTerm("000");
				if ((!ontology->concepts->getFact(concept.address))&&(concept!=newauxIDun)) {
					throw PluginError(
							"Tried to expand concept "
							+ RawPrinter::toString(reg, concept)
							+ ", which does not appear in the ontology");
				}
				ID individual = ogatom.tuple[2];
				DBGLOG(DBG,
						"Adding concept assertion: " << (ogatom.tuple[0] == query.input[2] ? "-" : "") << reg->terms.getByID(concept).getUnquotedString() << "(" << reg->terms.getByID(individual).getUnquotedString() << ")");
				TDLConceptExpression* factppConcept =
				ontology->kernel->getExpressionManager()->Concept(
						ontology->addNamespaceToString(
								reg->terms.getByID(concept).getUnquotedString()));
				if (ogatom.tuple.size() == 4)
				if (ogatom.tuple[3].address == 1)
				factppConcept =
				ontology->kernel->getExpressionManager()->Not(
						factppConcept);

				else if (ogatom.tuple[0] == query.input[2])
				factppConcept =
				ontology->kernel->getExpressionManager()->Not(
						factppConcept);
				addedAxioms.push_back(
						ontology->kernel->instanceOf(
								ontology->kernel->getExpressionManager()->Individual(
										ontology->addNamespaceToString(
												reg->terms.getByID(individual).getUnquotedString())),
								factppConcept));}
			} else if (ogatom.tuple[0] == query.input[3]
					|| ogatom.tuple[0] == query.input[4]) {
				// r+ or r-
				assert(
						ogatom.tuple.size() == 4
						&& "Second parameter must be a ternery predicate");
				ID role = ogatom.tuple[1];
				if (!ontology->roles->getFact(role.address)) {
					throw PluginError(
							"Tried to expand role "
							+ RawPrinter::toString(reg, role)
							+ ", which does not appear in the ontology");
				}
				ID individual1 = ogatom.tuple[2];
				ID individual2 = ogatom.tuple[3];
				DBGLOG(DBG,
						"Adding role assertion: " << (ogatom.tuple[0] == query.input[4] ? "-" : "") << reg->terms.getByID(role).getUnquotedString() << "(" << reg->terms.getByID(individual1).getUnquotedString() << ", " << reg->terms.getByID(individual2).getUnquotedString() << ")");
				TDLObjectRoleExpression* factppRole =
				ontology->kernel->getExpressionManager()->ObjectRole(
						ontology->addNamespaceToString(
								reg->terms.getByID(role).getUnquotedString()));

				if (ogatom.tuple[0] == query.input[4]) {
					addedAxioms.push_back(
							ontology->kernel->relatedToNot(
									ontology->kernel->getExpressionManager()->Individual(
											ontology->addNamespaceToString(
													reg->terms.getByID(individual1).getUnquotedString())),
									factppRole,
									ontology->kernel->getExpressionManager()->Individual(
											ontology->addNamespaceToString(
													reg->terms.getByID(individual2).getUnquotedString()))));
				} else {
					addedAxioms.push_back(
							ontology->kernel->relatedTo(
									ontology->kernel->getExpressionManager()->Individual(
											ontology->addNamespaceToString(
													reg->terms.getByID(individual1).getUnquotedString())),
									factppRole,
									ontology->kernel->getExpressionManager()->Individual(
											ontology->addNamespaceToString(
													reg->terms.getByID(individual2).getUnquotedString()))));
				}
			} else {
				assert(false && "Invalid input atom");
			}

			en++;
		}
		return addedAxioms;
	}

	void DLPluginAtom::restoreAbox(const Query& query,
			std::vector<TDLAxiom*> addedAxioms) {

		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0]);

		// remove the axioms again
		BOOST_FOREACH (TDLAxiom* ax, addedAxioms) {
			ontology->kernel->retract(ax);
		}
	}

	void DLPluginAtom::retrieve(const Query& query, Answer& answer) {
		assert(
				false
				&& "this method should never be called since the learning-based method is present");
	}

	void DLPluginAtom::learnSupportSets(const Query& query, NogoodContainerPtr nogoods) {

		DBGLOG(DBG, "LSS: learning support sets started");
		const ExternalAtom& eatom = query.ctx->registry()->eatoms.getByID(query.eatomID);

		// prepare variables for storing query and a nogood container
		std::string querystr;
		SimpleNogoodContainerPtr potentialSupportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());

		// case when the ontology is in el

		if (ctx.getPluginData<DLLitePlugin>().el) {

			// variable for storing maximal input
			std::map<ID,std::vector<ID> > maxinput;

			// prepare variables for storing ID of concept and role quaries
			RegistryPtr reg = getRegistry();
			ID cQID = ID_FAIL;
			ID rQID = ID_FAIL;

			ID cdlID = reg->storeConstantTerm("cDL");
			ID rdlID = reg->storeConstantTerm("rDL");

			// analyze whether the given DL-atom has a concept or a role as a DL-query
			if (eatom.predicate == cdlID) {
				cQID = eatom.inputs[5];
				querystr= RawPrinter::toString(reg,cQID);
				DBGLOG(DBG,"LSS: EL: the DL-query is a concept "<< querystr);
			}

			else if (eatom.predicate == rdlID) {
				rQID = eatom.inputs[5];
				querystr= RawPrinter::toString(reg,rQID);
				DBGLOG(DBG,"LSS: EL: the DL-query is a role "<< querystr);
			}

			else {
				assert(false);
			}

			// prepare output variable, tuple and negative output atom
			ID outvarID = reg->storeVariableTerm("O0");
			ID outvarID1 = reg->storeVariableTerm("O1");
			ID outvarID2 = reg->storeVariableTerm("O2");

			Tuple outlist;

			if (cQID != ID_FAIL) {
				outlist.push_back(outvarID);
			}

			else if (rQID != ID_FAIL) {
				outlist.push_back(outvarID1);
				outlist.push_back(outvarID2);
			}

			else {
				assert(false);
			}

			ID outlit = NogoodContainer::createLiteral(ExternalLearningHelper::getOutputAtom(query, outlist, false));
#ifndef NDEBUG
			std::string outlitStr = RawPrinter::toString(reg, outlit);
			DBGLOG(DBG, "LSS: EL: output atom is: " << outlitStr);
#endif

			// enlist ABox predicates
			DBGLOG(DBG, "LSS: EL: ABox predicates are: ");
			std::vector<ID> abp;
			std::string opath;
			if (ctx.getPluginData<DLLitePlugin>().repair) {
				abp =theDLLitePlugin.prepareOntology(ctx,reg->storeConstantTerm(ctx.getPluginData<DLLitePlugin>().repairOntology))->AboxPredicates;
				opath = std::string(theDLLitePlugin.prepareOntology(ctx,reg->storeConstantTerm(ctx.getPluginData<DLLitePlugin>().repairOntology))->ontologyPath);
			}
			else if (ctx.getPluginData<DLLitePlugin>().rewrite) {
				abp =theDLLitePlugin.prepareOntology(ctx,reg->storeConstantTerm(ctx.getPluginData<DLLitePlugin>().ontology))->AboxPredicates;
				opath = std::string(theDLLitePlugin.prepareOntology(ctx,reg->storeConstantTerm(ctx.getPluginData<DLLitePlugin>().ontology))->ontologyPath);
			}

			BOOST_FOREACH(ID id,abp) {
				DBGLOG(DBG, "LSS: EL:" << RawPrinter::toString(reg,id));
			}


			bm::bvector<>::enumerator en = eatom.getPredicateInputMask()->getStorage().first();
			bm::bvector<>::enumerator en_end = eatom.getPredicateInputMask()->getStorage().end();


			if (en<en_end) {
				DBGLOG(DBG,"LSS: EL: going through the maximal input ");
			}

			while (en < en_end) {
				// for each atom of the maximal input check if the predicate is c+ or r+ (c-, r- are not relevant if ontology is in EL)
#ifndef NDEBUG
				std::string enStr = RawPrinter::toString(reg,reg->ogatoms.getIDByAddress(*en));
				DBGLOG(DBG, "LSS: current input atom: " << enStr);
#endif
				const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

				if ((oatom.tuple[0] == query.input[1])||(oatom.tuple[0] == query.input[3])) {
					assert(oatom.tuple.size() == 3&& "Second parameter must be a binary predicate");
					// create a map which stores for each ontology predicate P a set of normal predicates, which yield the extension formed from P
					maxinput[oatom.tuple[1]].push_back(oatom.tuple[0]);
				}
				en++;
			}


			// getting support sets from Requiem tool
			FILE *in;
			char buff[512];
			std::string param = (cQID != ID_FAIL) ? std::string("(?0)") : std::string("(?0,?1)");
			std::string path = std::string(PLUGIN_DIR)+std::string("/requiem/dist/requiem-cli.jar");

			DBGLOG(DBG, "LSS: EL: the path to requiem is : " <<path);
			if (cQID != ID_FAIL) {
				std::string call = "java -Xmx1000M -jar "+path+" \"Q(?0)  <-  "+std::string(querystr)+param+"\" "+opath+" F";

				DBGLOG(DBG, "LSS: EL: sending call to Requiem " << call);

#ifdef WIN32
				assert(false && "requiem not supported under Windows");
#else
				if(!(in = popen(call.c_str(), "r"))) {
					assert(false&&"call to requiem failed");
				}

				bool computed_all_rewritings=true;
				bool get_further_rewritings=true;

				if (ctx.getPluginData<DLLitePlugin>().supnumber==0) {
					get_further_rewritings=false;
					computed_all_rewritings=false;
				}
				int number_of_considered_rewritings=0;	             
	
				while((fgets(buff, sizeof(buff), in)!=NULL)&&get_further_rewritings) {
					DBGLOG(DBG, "LSS: EL: got query rewriting from Requiem " << buff);
					if ((ctx.getPluginData<DLLitePlugin>().supnumber!=-1)&&(number_of_considered_rewritings>ctx.getPluginData<DLLitePlugin>().supnumber)) {
						DBGLOG(DBG,"LSS: EL: we stop computing further rewritings, the limit for the number of rewritings allowed for computation is reached");
						computed_all_rewritings=false;
						get_further_rewritings=false;
						break;
					}
				

					std::vector<std::string> strs;
					boost::split(strs, buff, boost::is_any_of("\t "), boost::token_compress_on);
					std::vector<std::string>::iterator row_it = strs.begin();
					std::vector<std::string>::iterator row_end = strs.end();

					DBGLOG(DBG, "LSS: EL: start parsing the rewriting");
					bool sup=true;
					Nogood supportset;

					// vector for storing ontology predicates that are relevant for the current support set
					std::vector<dlvhex::ID> ont;

					// vector for storing input predicates that are relevant for the current support set
					std::vector<dlvhex::ID> inp;

					// temporary vectors
					std::vector<dlvhex::ID> ontinp;
					std::vector<std::pair<dlvhex::ID,int> > temp;

					// vector for storing support sets constructed from the current rewriting
					std::vector<std::vector<dlvhex::ID> > ngset;

					int rewriting_size=0;
					while (row_it<row_end) {

						std::string str (*row_it);
						DBGLOG(DBG, "LSS: EL: current atom of the rewriting is: " << *row_it);
						rewriting_size++;

						if ((ctx.getPluginData<DLLitePlugin>().supsize!=-1)&&(rewriting_size>ctx.getPluginData<DLLitePlugin>().supsize)) {
							computed_all_rewritings=false;
							DBGLOG(DBG,"LSS: EL: current rewriting exceeds the support size limit, we break");
							sup = false;
							break;
						}

						std::size_t varb = str.find("(");
						std::size_t vare = str.find(")");
						std::size_t varm = str.find(",");
						std::string var1;
						std::string var2;
						bool r=false;
						std::string pred;

						if (varb!=std::string::npos) {
							pred = str.substr(0,varb);
						}

						else {assert(false&&"output from requiem has a form that is not expected");
						}

						pred = "\""+pred+"\"";
						DBGLOG(DBG, "LSS: EL: quoted predicate name is " << pred<<" check its relevance");
						ID opID = reg->terms.getIDByString(pred);


						// check whether the obtained predicate occurs in the ABox
						if ((std::find(abp.begin(), abp.end(), opID) == abp.end())&&(maxinput.find(reg->terms.getIDByString(pred))==maxinput.end()))
						{
							// the element is not relevant
							DBGLOG(DBG, "LSS: EL: predicate "<<pred<<" does not occur in either of the ABox or the maximum input, skip the rewriting");
							sup = false;
							break;
						}
						else // the element of the rewriting is relevant for the given DL-atom
						{
							DBGLOG(DBG,"LSS: EL: the predicate "<< pred<< "is relevant");

							// declare variables that will be used in the nogoods
							ID var1ID;
							ID var2ID;

							// set the variables that participate in the nogoods
							if (varm!=std::string::npos) {
								r=true;
								std::string var1 = str.substr (varb+1,varm-varb-1);
								std::string var2 = str.substr (varm+1,vare-varm-1);
								DBGLOG(DBG, "LSS: EL: this is a role predicate ");
								var1ID = reg->terms.getIDByString(var1);
								if (var1ID==ID_FAIL) var1ID = reg->storeVariableTerm(var1);
								var2ID = reg->terms.getIDByString(var2);
								if (var2ID==ID_FAIL) var2ID = reg->storeVariableTerm(var2);

							}
							else {
								std::string var1 = str.substr (varb+1,vare-varb-1);
								DBGLOG(DBG,  "LSS: EL: this is a concept predicate");
								var1ID = reg->terms.getIDByString(var1);
								if (var1ID==ID_FAIL) var1ID = reg->storeVariableTerm(var1);
							}

							// variable for storing ID of the created atom if the predicate occurs in the ABox
							dlvhex::ID ia=ID_FAIL;

							// variable for storing ID of the created atom if the predicate occurs in the input
							dlvhex::ID ip=ID_FAIL;

							// check whether the current predicate of the rewriting occurs in the ABox
							if (std::find(abp.begin(), abp.end(), opID) != abp.end()) {
								DBGLOG(DBG,"LSS: EL: the predicate occurs in the ABox");

								// create a guard atom which will be part of a support set
								OrdinaryAtom gatom = theDLLitePlugin.getNewGuardAtom();
								gatom.tuple.push_back(opID);
								gatom.tuple.push_back(var1ID);
								if (r) gatom.tuple.push_back(var2ID);

								ia = NogoodContainer::createLiteral(reg->storeOrdinaryAtom(gatom));

							    DBGLOG(DBG,"LSS: EL: the atom created for support set is "<< RawPrinter::toString(reg,ia));

							}

							ont.push_back(ia);
							// check whether the predicate occurs in the maximal input

							if (maxinput.find(reg->terms.getIDByString(pred))!=maxinput.end()) {
								DBGLOG(DBG,"LSS: EL:  the predicate occurs in the input");
								OrdinaryAtom rp = (r ? theDLLitePlugin.getNewAtom(query.input[3], true):theDLLitePlugin.getNewAtom(query.input[1], true));
								if (!r) {
								    rp = theDLLitePlugin.getNewAtom(query.input[1], true);
									rp.tuple.push_back(opID);
									rp.tuple.push_back(var1ID);
								}
								else {
									rp = theDLLitePlugin.getNewAtom(query.input[3], true);
									rp.tuple.push_back(opID);
									rp.tuple.push_back(var1ID);
									rp.tuple.push_back(var2ID);
								}

								ip = NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rp));
								DBGLOG(DBG,"LSS: EL: the atom created for support set is "<< RawPrinter::toString(reg,ip));
							}

							inp.push_back(ip);
						}
						row_it++;
					}

					for(std::vector<std::string>::size_type k = 0; k != ont.size(); k++) {
							if (ont[k]==ID_FAIL) {
								ontinp.push_back(inp[k]);
							} else if (inp[k]==ID_FAIL) {
								ontinp.push_back(ont[k]);
							}
							else {
								ontinp.push_back(ont[k]);
								temp.push_back(std::pair<dlvhex::ID,int>(inp[k],k));
							}
						}


					for (std::vector<dlvhex::ID>::size_type t = 0; t != temp.size(); t++){
						DBGLOG(DBG,"LSS: EL: temp[" << t << "] = "<<RawPrinter::toString(reg,temp[t].first));
					}

					int pi = 1;
					for (int i=0;i <temp.size();++i) pi *= 2;
					for (int i=0; i<pi; i++) {
						DBGLOG(DBG,"LSS: EL: start constructing combinations of ontology and input predicates for support sets");

							std::vector<dlvhex::ID> s=ontinp;

							for (int m=0; m<temp.size(); m++) {
					   			 if ((i >> m) % 2 == 1)
									s[temp[m].second]=temp[m].first;
							}

							for (std::vector<dlvhex::ID>::size_type t = 0; t != s.size(); t++) {
								DBGLOG(DBG,"LSS: EL: s["<<t<<"] = "<<RawPrinter::toString(reg,s[t]));
							}

							ngset.push_back(s);
					}

					DBGLOG(DBG,"LSS: EL: number of constructed support sets: "<<ngset.size());


					for (std::vector<std::vector<dlvhex::ID> >::size_type l = 0; l != ngset.size(); l++){
						DBGLOG(DBG,"LSS: EL: ngset number "<< l <<" is ");
						for (std::vector<dlvhex::ID>::size_type k = 0; k != ngset[l].size(); k++){
							DBGLOG(DBG,"LSS: EL: "<<RawPrinter::toString(reg,ngset[l][k]));
						}
					}




					if (sup) {
						DBGLOG(DBG,"LSS: EL: there were "<< ngset.size()<<" support sets created from this rewriting");

						for (std::vector<std::vector<dlvhex::ID> >::size_type t = 0; t != ngset.size(); t++) {
							Nogood supset;
							supset.insert(outlit);
							// construct literal from a given atom and add it to nogood
							for (std::vector<dlvhex::ID>::size_type k = 0; k != ngset[t].size(); k++) {
								supset.insert(NogoodContainer::createLiteral(ngset[t][k]));
							}
							DBGLOG(DBG,"LSS: EL: -->adding support set "<<supset.getStringRepresentation(reg));

							potentialSupportSets->addNogood(supset);
							// construction of a support set is finished, add it to the set of all support sets
						}
					}
					number_of_considered_rewritings++;

				}

				if (!computed_all_rewritings) {
					DBGLOG(DBG,"LSS: EL: There is no evidence for support family completeness");
					DBGLOG(DBG,"LSS: EL: Thus we add "<<RawPrinter::toString(reg,cQID)<<" the current atom to the set of atoms not known to be completely supported");
					ctx.getPluginData<DLLitePlugin>().incompletedlat.push_back(cQID);
				}

				pclose(in);
#endif
			}
			else if (rQID != ID_FAIL) {
				DBGLOG(DBG, "LSS: EL: the query is a role, thus we do not call the Requeim tool");
				if (std::find(abp.begin(), abp.end(), rQID) != abp.end()) {
					Nogood s;
					OrdinaryAtom gatom = theDLLitePlugin.getNewGuardAtom();
					gatom.tuple.push_back(rQID);
					gatom.tuple.push_back(outvarID1);
					gatom.tuple.push_back(outvarID2);
					s.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(gatom)));
					s.insert(outlit);
					potentialSupportSets->addNogood(s);
					DBGLOG(DBG, "LSS: EL: adding support set "<< s.getStringRepresentation(reg));
				}

				if (maxinput.find(rQID)!=maxinput.end()) {
					DBGLOG(DBG, "LSS: EL: create support sets for input atoms, there are "<<maxinput[rQID].size()<<" relevant atoms ");
					BOOST_FOREACH(ID id1,maxinput[rQID]) {
						Nogood s1;
						OrdinaryAtom newa = theDLLitePlugin.getNewAtom(rQID);
						newa.tuple.push_back(outvarID1);
						newa.tuple.push_back(outvarID2);
						s1.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(newa)));
						s1.insert(outlit);
						potentialSupportSets->addNogood(s1);
						DBGLOG(DBG, "LSS: EL: adding support set "<< s1.getStringRepresentation(reg)<<"\n");
					}
				}

			}
			else assert(false);

			DBGLOG(DBG, "LSS: EL: construct missing support sets, if there are any");
			SimpleNogoodContainerPtr additionalSupportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());
			for (int i = 0; i<potentialSupportSets->getNogoodCount();i++) {
				DBGLOG(DBG,"LSS: EL: consider support set number "<< i <<": " << potentialSupportSets->getNogood(i).getStringRepresentation(reg));
				const Nogood& ng = potentialSupportSets->getNogood(i);
				DBGLOG(DBG,"LSS: EL: check maximal input");
				BOOST_FOREACH (ID id, ng) {
					/*if (id.isOrdinaryGroundAtom()) {
						=reg->ogatoms.getByID(id); DBGLOG(DBG,"LSS: ground");
					}
					else if (id.isOrdinaryNongroundAtom){
						const OrdinaryAtom& oa=reg->onatoms.getByID(id); DBGLOG(DBG,"LSS: nonground");
					}*/
					const OrdinaryAtom& oa = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByID(id) : reg->onatoms.getByID(id));
					DBGLOG(DBG,"EL: current element: "<<RawPrinter::toString(reg,id));
					DBGLOG(DBG,"EL: does "<<RawPrinter::toString(reg,oa.tuple[1])<<" occur in the maximal input?");

					if (maxinput.find(oa.tuple[1])!=maxinput.end()) {
						DBGLOG(DBG,"LSS: EL: "<<RawPrinter::toString(reg,oa.tuple[1])<<" occurs in maximal input "<< maxinput[oa.tuple[1]].size()<<" new support and add it");
						//go through elements of maxinput[oa.tuple[0]] and for each of them add a new support set

						BOOST_FOREACH(ID id1,maxinput[oa.tuple[1]]) {
							Nogood sup;
							OrdinaryAtom newa = theDLLitePlugin.getNewAtom(id1);
							newa.tuple.push_back(oa.tuple[1]);
							newa.tuple.push_back(oa.tuple[2]);
							if (oa.tuple.size()==4)
							newa.tuple.push_back(oa.tuple[3]);
							//DBGLOG(DBG, "EL: atom is created"<<RawPrinter::toString(reg,newa.tuple[0])<<" "<<RawPrinter::toString(reg,newa.tuple[1])<<" "<<RawPrinter::toString(reg,newa.tuple[2]));
							BOOST_FOREACH (ID id2, ng) {
								if (id2!=id) {
									//DBGLOG(DBG, "EL: add the existing element");
									sup.insert(id2);
								}
								else {
									sup.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(newa)));
									//DBGLOG(DBG, "EL: add a new element ");
								}
							}
							additionalSupportSets->addNogood(sup);
							DBGLOG(DBG, "LSS: EL: adding support set with input elements: " << sup.getStringRepresentation(reg));
						}

					}

				}

			}

			for (int i = 0; i<additionalSupportSets->getNogoodCount();i++) {
				potentialSupportSets->addNogood(additionalSupportSets->getNogood(i));
			}

			DBGLOG(DBG, "LSS: EL: "<<potentialSupportSets->getNogoodCount()<<" support sets learned");
			for (int i = 0; i < potentialSupportSets->getNogoodCount(); i++) {
				const Nogood& ng = potentialSupportSets->getNogood(i);
				DBGLOG(DBG, "LSS: EL: " << ng.getStringRepresentation(reg));
			}
		}


		// ontology is in DLLite
		else {
			// make sure that the ontology is in the cache and retrieve its classification
			DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(ctx, query.input[0]);

			// classify the Tbox if not already done
			if (!ontology->classification)
			ontology->computeClassification(ctx);
			InterpretationPtr classification = ontology->classification;

#ifndef NDEBUG
			DBGLOG(DBG,
					"LSS: Using the following model CM of the classification program: CM = " << *classification);
#endif
			RegistryPtr reg = getRegistry();

			ID cQID = ID_FAIL;
			ID rQID = ID_FAIL;

			ID cdlID = reg->storeConstantTerm("cDL");
			ID rdlID = reg->storeConstantTerm("rDL");

			if (eatom.predicate == cdlID) {
				cQID = eatom.inputs[5];
				DBGLOG(DBG,"LSS: query is a concept");
			}

			else if (eatom.predicate == rdlID) {
				rQID = eatom.inputs[5];
				DBGLOG(DBG,"LSS: query is a role");
			}

			else {
				assert(false);
			}
			// prepare output variable, tuple and negative output atom
			DBGLOG(DBG, "Storing output atom which will be part of any support set");
			ID outvarID = reg->storeVariableTerm("O");
			ID outvarID1 = reg->storeVariableTerm("O1");
			ID outvarID2 = reg->storeVariableTerm("O2");
			Tuple outlist;

			if (cQID != ID_FAIL) {
				DBGLOG(DBG, "Query is a concept");
				outlist.push_back(outvarID);
			}

			else if (rQID != ID_FAIL) {
				DBGLOG(DBG, "Query is a role");
				outlist.push_back(outvarID1);
				outlist.push_back(outvarID2);
			}

			else {
				assert(false);
			}

			ID outlit = NogoodContainer::createLiteral(ExternalLearningHelper::getOutputAtom(query, outlist, false));
#ifndef NDEBUG
			std::string outlitStr = RawPrinter::toString(reg, outlit);
			DBGLOG(DBG, "LSS: IPM: Output atom is " << outlitStr);
#endif



			// iterate over the maximum input
			DBGLOG(DBG, "LSS: Analyzing input to the external atom");


			bm::bvector<>::enumerator en = eatom.getPredicateInputMask()->getStorage().first();
			bm::bvector<>::enumerator en_end = eatom.getPredicateInputMask()->getStorage().end();



			ID qID = query.input[5];

#define DBGCHECKATOM(id) { std::stringstream ss; RawPrinter printer(ss, reg); printer.print(id); DBGLOG(DBG, "LSS: Checking if " << ss.str() << " holds in CM:"); }

			while (en < en_end) {
				// check if it is c+, c-, r+ or r-

#ifndef NDEBUG
				std::string enStr = RawPrinter::toString(reg,reg->ogatoms.getIDByAddress(*en));
				DBGLOG(DBG, "LSS: IPM: Current input atom: " << enStr);
#endif
				const OrdinaryAtom& oatom = reg->ogatoms.getByAddress(*en);

				if (oatom.tuple[0] == query.input[1]) {
					// c+
					DBGLOG(DBG, "LSS: Atom belongs to c+");
					assert(oatom.tuple.size() == 3&& "Second parameter must be a binary predicate");
					ID cID = oatom.tuple[1];

					// check if sub(C, Q) is true in the classification assignment
					OrdinaryAtom subcq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					subcq.tuple.push_back(theDLLitePlugin.subID);
					subcq.tuple.push_back(cID);
					subcq.tuple.push_back(qID);
					ID subcqID = reg->storeOrdinaryAtom(subcq);

					DBGCHECKATOM(subcqID)
					if (classification->getFact(subcqID.address)) {
						OrdinaryAtom cpcx = theDLLitePlugin.getNewAtom(query.input[1]);
						cpcx.tuple.push_back(cID);
						cpcx.tuple.push_back(outvarID);
						Nogood supportset;
						supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcx)));
						supportset.insert(outlit);
						DBGLOG(DBG,"LSS: Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
						potentialSupportSets->addNogood(supportset);
					} else {
						DBGLOG(DBG, "LSS: Does not hold");
					}

					// check if conf(C, C) is true in the classification assignment
					OrdinaryAtom confcc(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					confcc.tuple.push_back(theDLLitePlugin.confID);
					confcc.tuple.push_back(cID);
					confcc.tuple.push_back(cID);
					ID confccID = reg->storeOrdinaryAtom(confcc);

					DBGCHECKATOM(confccID)
					if (classification->getFact(confccID.address)) {
						OrdinaryAtom cpcx = theDLLitePlugin.getNewAtom(query.input[1]);
						cpcx.tuple.push_back(cID);
						cpcx.tuple.push_back(theDLLitePlugin.xID);
						Nogood supportset;
						supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcx)));
						supportset.insert(outlit);
						DBGLOG(DBG,
								"LSS: Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
						potentialSupportSets->addNogood(supportset);
					} else {
						DBGLOG(DBG, "LSS: Does not hold");
					}

					// check if conf(C, C') is true in the classification model (for some C')
#ifndef NDEBUG
					std::string cStr = RawPrinter::toString(reg, cID);

					DBGLOG(DBG,"LSS: Checking if conf(" << cStr << ", C') is true in the classification assignment (for some C')");
#endif
					bm::bvector<>::enumerator en2 = classification->getStorage().first();
					bm::bvector<>::enumerator en2_end =	classification->getStorage().end();
					while (en2 < en2_end) {
#ifndef NDEBUG
						std::string en2Str = RawPrinter::toString(reg,reg->ogatoms.getIDByAddress(*en2));
						DBGLOG(DBG, "LSS: Current classification atom: " << en2Str);
#endif
						const OrdinaryAtom& clAtom = reg->ogatoms.getByAddress(*en2);
						if (clAtom.tuple[0] == theDLLitePlugin.confID && clAtom.tuple[1] == cID) {
							ID cpID = clAtom.tuple[2];
#ifndef NDEBUG
							std::string cpStr = RawPrinter::toString(reg, cpID);
							DBGLOG(DBG,"LSS: Found a match with C=" << cStr << " and C'=" << cpStr);
#endif

							// add {c+(C, Y), C'(Y)}
							Nogood supportset;

							OrdinaryAtom cpcy = theDLLitePlugin.getNewAtom(query.input[1]);
							cpcy.tuple.push_back(cID);
							cpcy.tuple.push_back(theDLLitePlugin.yID);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

							// guard atom
							// since we cannot check guard atoms of form exR(Y), we rewrite them to R(Y,Z)

							ID guardID;
							if (theDLLitePlugin.isDlEx(cpID)) {
								ID negnoexcpID = theDLLitePlugin.dlRemoveEx(clAtom.tuple[2]);
								OrdinaryAtom negnoexcp = theDLLitePlugin.getNewGuardAtom();
								negnoexcp.tuple.push_back(negnoexcpID);
								negnoexcp.tuple.push_back(theDLLitePlugin.yID);
								negnoexcp.tuple.push_back(theDLLitePlugin.zID);
								guardID = reg->storeOrdinaryAtom(negnoexcp);
								supportset.insert(NogoodContainer::createLiteral(guardID));
							} else {
								ID negcpID = clAtom.tuple[2];
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
							DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg) << " where " << negcpStr << " is the guard atom " << guardStr);
#endif

							potentialSupportSets->addNogood(supportset);

							// check if c-(C', Y) occurs in the maximal interpretation
							bm::bvector<>::enumerator en3 = eatom.getPredicateInputMask()->getStorage().first();
							bm::bvector<>::enumerator en3_end = eatom.getPredicateInputMask()->getStorage().end();

#ifndef NDEBUG
							DBGLOG(DBG,"LSS: Checking if (C',Y) with C'=" << RawPrinter::toString(reg, theDLLitePlugin.dlNeg(cpID)) << " occurs in c- (for some Y)");
#endif
							while (en3 < en3_end) {
								const OrdinaryAtom& at = reg->ogatoms.getByAddress(*en3);
								if (at.tuple[0] == query.input[2] && at.tuple[1] == theDLLitePlugin.dlNeg(clAtom.tuple[2])) {
#ifndef NDEBUG
									std::string yStr = RawPrinter::toString(reg,at.tuple[2]);
									DBGLOG(DBG,"LSS: --> Found a match with Y=" << yStr);
#endif
									Nogood supportset;

									// add { T c+(C,Y), T c-(C,Y) }
									OrdinaryAtom cpcy = theDLLitePlugin.getNewAtom(query.input[1], false);
									cpcy.tuple.push_back(cID);
									cpcy.tuple.push_back(theDLLitePlugin.yID);
									//cpcy.tuple.push_back(at.tuple[2]);
									supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cpcy)));

									OrdinaryAtom cmcy = theDLLitePlugin.getNewAtom(query.input[2], false);
									cmcy.tuple.push_back(theDLLitePlugin.dlNeg(cpID));
									cmcy.tuple.push_back(theDLLitePlugin.yID);
									supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cmcy)));

									supportset.insert(outlit);

									DBGLOG(DBG,"LSS: --> !Learned support set: " << supportset.getStringRepresentation(reg));
									potentialSupportSets->addNogood(supportset);
									break;
								}
								en3++;
							}
							DBGLOG(DBG,
									"LSS: Finished checking if (C',Y) with C'=" << cpStr << " occurs in c- (for some Y)");
						}
						en2++;
					}
					DBGLOG(DBG,"LSS: Finished checking if conf(" << cStr << ", C') is true in the classification assignment (for some C')");

				} else if (oatom.tuple[0] == query.input[2]) {
					// c-
					DBGLOG(DBG, "LSS: Atom belongs to c-");
					assert(oatom.tuple.size() == 3&& "Third parameter must be a binary predicate");

					ID cID = oatom.tuple[1];
					ID guardID;

					Nogood supportset;

					// add { T c-(C,Y), C(Y) }
					OrdinaryAtom cmcy = theDLLitePlugin.getNewAtom(query.input[2], false);
					cmcy.tuple.push_back(cID);
					cmcy.tuple.push_back(theDLLitePlugin.yID);
					supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cmcy)));

					ID cyID = theDLLitePlugin.dlNeg(cID);
					OrdinaryAtom cy = theDLLitePlugin.getNewGuardAtom();
					cy.tuple.push_back(cyID);
					cy.tuple.push_back(theDLLitePlugin.yID);
					guardID = reg->storeOrdinaryAtom(cy);
					supportset.insert(NogoodContainer::createLiteral(guardID));

					supportset.insert(outlit);
					DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
					potentialSupportSets->addNogood(supportset);

				} else if (oatom.tuple[0] == query.input[3]) {
					// r+
					DBGLOG(DBG, "LSS: Atom belongs to r+");
					assert(oatom.tuple.size() == 4&& "Fourth parameter must be a ternary predicate");

					ID rID = oatom.tuple[1];
					ID exrID = theDLLitePlugin.dlEx(rID);

					#ifndef NDEBUG
						std::string rIDStr = RawPrinter::toString(reg, rID);
						std::string exrIDStr = RawPrinter::toString(reg, exrID);
					#endif

					if (cQID!=ID_FAIL) {
						// check if sub(exR, Q) classification assignment (applicable only for concepts)
						OrdinaryAtom subexrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
						subexrq.tuple.push_back(theDLLitePlugin.subID);
						subexrq.tuple.push_back(theDLLitePlugin.dlEx(rID));
						subexrq.tuple.push_back(qID);
						ID subexrqID = reg->storeOrdinaryAtom(subexrq);

						DBGCHECKATOM(subexrqID)
						if (classification->getFact(subexrqID.address)) {
							OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
							rprxy.tuple.push_back(rID);
							rprxy.tuple.push_back(outvarID);
							rprxy.tuple.push_back(theDLLitePlugin.yID);
							Nogood supportset;
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
							supportset.insert(outlit);
							DBGLOG(DBG,"LSS: Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
							potentialSupportSets->addNogood(supportset);
						} else {
							DBGLOG(DBG, "LSS: Does not hold");
						}
					}

					else {
						// check if sub(R, Q) is true in the classification assignment (applicable only for roles)
						OrdinaryAtom subrq(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
						subrq.tuple.push_back(theDLLitePlugin.subID);
						subrq.tuple.push_back(rID);
						subrq.tuple.push_back(qID);
						ID subrqID = reg->storeOrdinaryAtom(subrq);

						DBGCHECKATOM(subrqID)
						if (classification->getFact(subrqID.address)) {
							OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[1]);
							rprxy.tuple.push_back(rID);
							rprxy.tuple.push_back(outvarID1);
							rprxy.tuple.push_back(outvarID2);
							Nogood supportset;
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
							supportset.insert(outlit);
							DBGLOG(DBG,"LSS: Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
							potentialSupportSets->addNogood(supportset);
						} else {
							DBGLOG(DBG, "LSS: Does not hold");
						}
					}



					// check if confref(R) is true in the classification assignment (applicable only for roles)
					OrdinaryAtom confrefr(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					confrefr.tuple.push_back(theDLLitePlugin.confrefID);
					confrefr.tuple.push_back(rID);
					ID confrefrID = reg->storeOrdinaryAtom(confrefr);

					DBGCHECKATOM(confrefrID)
					if (classification->getFact(confrefrID.address)) {
						OrdinaryAtom rprref = theDLLitePlugin.getNewAtom(query.input[3]);
						rprref.tuple.push_back(rID);
						rprref.tuple.push_back(theDLLitePlugin.yID);
						rprref.tuple.push_back(theDLLitePlugin.yID);
						Nogood supportset;
						supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprref)));
						supportset.insert(outlit);
						DBGLOG(DBG,"LSS: Holds --> Learned support set: " << supportset.getStringRepresentation(reg));
						potentialSupportSets->addNogood(supportset);
					} else {
						DBGLOG(DBG, "LSS: Does not hold");
					}


					// check if funct(R) is true in the classification assignment (applicable only for roles)
					OrdinaryAtom functr(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
					functr.tuple.push_back(theDLLitePlugin.functID);
					functr.tuple.push_back(rID);
					ID functrID = reg->storeOrdinaryAtom(functr);

					DBGCHECKATOM(functrID)

					// add two support sets: {r+(R,X,Y), r+(R,X,Z)}, {r+(R,X,Y), R(X,Z)}
					if (classification->getFact(functrID.address)) {
						OrdinaryAtom rprfunct = theDLLitePlugin.getNewAtom(query.input[3]);
						rprfunct.tuple.push_back(rID);
						rprfunct.tuple.push_back(theDLLitePlugin.xID);
						rprfunct.tuple.push_back(theDLLitePlugin.yID);

						OrdinaryAtom rprfunct2 = theDLLitePlugin.getNewAtom(query.input[3]);
						rprfunct.tuple.push_back(rID);
						rprfunct.tuple.push_back(theDLLitePlugin.xID);
						rprfunct.tuple.push_back(theDLLitePlugin.zID);

						OrdinaryAtom rfunct = theDLLitePlugin.getNewGuardAtom();
						rfunct.tuple.push_back(rID);
						rfunct.tuple.push_back(theDLLitePlugin.xID);
						rfunct.tuple.push_back(theDLLitePlugin.zID);

						Nogood supportset;
						supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprfunct)));
						supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprfunct2)));
						supportset.insert(outlit);
						DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
						potentialSupportSets->addNogood(supportset);

						Nogood supportset2;
						supportset2.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprfunct)));
						supportset2.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rfunct)));
						supportset2.insert(outlit);
						DBGLOG(DBG,"LSS: --> Learned support set: " << supportset2.getStringRepresentation(reg));
						potentialSupportSets->addNogood(supportset2);


					} else {
							DBGLOG(DBG, "LSS: Does not hold");
					}


					#ifndef NDEBUG
						std::string cStr=RawPrinter::toString(reg,exrID);
						std::string cStr2=RawPrinter::toString(reg,rID);
						DBGLOG(DBG,"LSS: Checking if conf("<<cStr<<",C') holds in CM for some C' or conf("<<cStr2<<",R') holds for some R'");
					#endif

					bm::bvector<>::enumerator en2 = classification->getStorage().first();
					bm::bvector<>::enumerator en2_end =	classification->getStorage().end();
					while (en2 < en2_end) {
						const OrdinaryAtom& at = reg->ogatoms.getByAddress(*en2);
						if (at.tuple[0] == theDLLitePlugin.confID && at.tuple[1] == exrID) {
							DBGLOG(DBG,"LSS: Found a match for conf("<<RawPrinter::toString(reg,exrID)<<",C') with C'=" << RawPrinter::toString(reg, at.tuple[2]));
							DBGLOG(DBG,"LSS: Check whether C' is a concept");

							if ((theDLLitePlugin.isDlNeg(at.tuple[2]) && theDLLitePlugin.isDlEx(theDLLitePlugin.dlNeg(at.tuple[2])))||(theDLLitePlugin.isDlEx(at.tuple[2]))) {
								DBGLOG(DBG,"LSS: C' is of form exR or -exR, ignore it");
							} else {
								DBGLOG(DBG,"LSS: C' is a concept, create a support set");
								Nogood supportset;
								// add { T r+(R,X,Y), C(X) }
								OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
								rprxy.tuple.push_back(rID);
								rprxy.tuple.push_back(theDLLitePlugin.xID);
								rprxy.tuple.push_back(theDLLitePlugin.yID);
								supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));

								// guard atom
								ID cgID = at.tuple[2];
								OrdinaryAtom cg = theDLLitePlugin.getNewGuardAtom();
								cg.tuple.push_back(cgID);
								cg.tuple.push_back(theDLLitePlugin.xID);
								ID cgatID = reg->storeOrdinaryAtom(cg);
								supportset.insert(NogoodContainer::createLiteral(cgatID));

								supportset.insert(outlit);

								DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
								potentialSupportSets->addNogood(supportset);
								if (theDLLitePlugin.isDlNeg(cgID)) {
									bm::bvector<>::enumerator en3 = eatom.getPredicateInputMask()->getStorage().first();
									bm::bvector<>::enumerator en3_end = eatom.getPredicateInputMask()->getStorage().end();
									while (en3 < en3_end) {
											const OrdinaryAtom& inat = reg->ogatoms.getByAddress(*en3);
											if (inat.tuple[0] == query.input[2]&& inat.tuple[1] == theDLLitePlugin.dlNeg(cgID)) {
												DBGLOG(DBG,"LSS: Found a match for c-(inv(C'),X)");
												Nogood supportset2;
												// add { T r+(R,X,Y), T C-(X) }
												supportset2.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));
												OrdinaryAtom cx = theDLLitePlugin.getNewAtom(query.input[2]);
												cx.tuple.push_back(theDLLitePlugin.dlNeg(cgID));
												rprxy.tuple.push_back(theDLLitePlugin.xID);
												supportset2.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(cx)));

												supportset2.insert(outlit);

												DBGLOG(DBG,"LSS: --> Learned support set: " << supportset2.getStringRepresentation(reg));

											}
											en3++;
									}
								}
							}
						}

						if (at.tuple[0] == theDLLitePlugin.confID && at.tuple[1] == rID) {
							DBGLOG(DBG,"LSS: --> Found a match for conf("<<RawPrinter::toString(reg,rID)<<",R') with R'=" << RawPrinter::toString(reg, at.tuple[2]));
							Nogood supportset;

							// add { T r+(R,X,Y), R'(X,Y) }
							OrdinaryAtom rprxy = theDLLitePlugin.getNewAtom(query.input[3]);
							rprxy.tuple.push_back(rID);
							rprxy.tuple.push_back(theDLLitePlugin.xID);
							rprxy.tuple.push_back(theDLLitePlugin.yID);
							supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(rprxy)));

							// guard atom
							ID rgID = at.tuple[2];
							OrdinaryAtom rg = theDLLitePlugin.getNewGuardAtom();
							rg.tuple.push_back(rgID);
							rg.tuple.push_back(theDLLitePlugin.xID);
							rg.tuple.push_back(theDLLitePlugin.yID);
							ID rgatID = reg->storeOrdinaryAtom(rg);
							supportset.insert(NogoodContainer::createLiteral(rgatID));

							supportset.insert(outlit);

							DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
							potentialSupportSets->addNogood(supportset);

							// if R' is negative, then check whether r-(R',X,Y) holds in the maximum input
							if (theDLLitePlugin.isDlNeg(rgID)){

								bm::bvector<>::enumerator en3 = eatom.getPredicateInputMask()->getStorage().first();
								bm::bvector<>::enumerator en3_end = eatom.getPredicateInputMask()->getStorage().end();
								while (en3 < en3_end) {
									const OrdinaryAtom& inat = reg->ogatoms.getByAddress(*en3);
									if (inat.tuple[0] == query.input[4]&& inat.tuple[1] == rgID) {
										DBGLOG(DBG,"LSS: ");
									}
									en3++;
								}
							}

						}

						en2++;
					}



		} else if (oatom.tuple[0] == query.input[4]) {
			// r-
			DBGLOG(DBG, "LSS: Atom belongs to r-");
			assert(oatom.tuple.size() == 4&& "Fifth parameter must be a ternary predicate");
			ID rID = oatom.tuple[1];

		}
		en++;
	}

			DBGLOG(DBG, "LSS: Analyzing query and Abox");
			{
				ID qID = query.input[5];

				// guard atom for Q(Y)
				OrdinaryAtom qy = theDLLitePlugin.getNewGuardAtom();
				qy.tuple.push_back(query.input[5]);

				if (cQID != ID_FAIL) {
					DBGLOG(DBG, "Query is a concept");
					qy.tuple.push_back(outvarID);
				} else if (rQID != ID_FAIL) {
					DBGLOG(DBG, "Query is a role");
					qy.tuple.push_back(outvarID1);
					qy.tuple.push_back(outvarID2);
					// DBGLOG(DBG,"Here is guard atom for it: "<<RawPrinter::toString(reg, reg->ogatoms.getIDByStorage(qy)));

					//DBGLOG(DBG,"Guard atom for role is: "<<RawPrinter::toString(reg, reg->onatoms.getIDByStorage(qy));
				} else {
					DBGLOG(DBG, "Query is neither a concept nor a role");
				}

				Nogood supportset;
				supportset.insert(
						NogoodContainer::createLiteral(reg->storeOrdinaryAtom(qy)));
				supportset.insert(outlit);
				DBGLOG(DBG,
						"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
				potentialSupportSets->addNogood(supportset);

				// check if sub(C, Q) or sub(R,Q) is true in the classification assignment (for some C)
#ifndef NDEBUG
				std::string qstr = RawPrinter::toString(reg, query.input[5]);

				DBGLOG(DBG,
						"LSS: Checking if sub(C, " << qstr << ") is true in the classification assignment (for some C')");
#endif
				bm::bvector<>::enumerator en = classification->getStorage().first();
				bm::bvector<>::enumerator en_end = classification->getStorage().end();
				while (en < en_end) {
					DBGLOG(DBG,
							"LSS: Current classification atom: " << RawPrinter::toString(reg, reg->ogatoms.getIDByAddress(*en)));
					const OrdinaryAtom& clAtom = reg->ogatoms.getByAddress(*en);
					if (clAtom.tuple[0] == theDLLitePlugin.subID
							&& clAtom.tuple[2] == qID) {
						ID cID = clAtom.tuple[1];
#ifndef NDEBUG
						DBGLOG(DBG,
								"LSS: Found a match with C=" << RawPrinter::toString(reg, cID));
#endif
						if (theDLLitePlugin.isDlEx(clAtom.tuple[1])) {
							DBGLOG(DBG, "LSS: (this is form exR)");
							// guard atom for C(O,Y)
							OrdinaryAtom roy = theDLLitePlugin.getNewGuardAtom();
							roy.tuple.push_back(theDLLitePlugin.dlRemoveEx(cID));
							roy.tuple.push_back(outvarID);
							roy.tuple.push_back(theDLLitePlugin.yID);
							Nogood supportset;
							supportset.insert(
									NogoodContainer::createLiteral(
											reg->storeOrdinaryAtom(roy)));
							supportset.insert(outlit);
							DBGLOG(DBG,
									"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
							potentialSupportSets->addNogood(supportset);
						} else {
							DBGLOG(DBG, "LSS: (this is not of form exR)");
							if (cQID != ID_FAIL) {
								// guard atom for C(O)
								OrdinaryAtom co = theDLLitePlugin.getNewGuardAtom();
								co.tuple.push_back(cID);
								co.tuple.push_back(outvarID);
								Nogood supportset;
								supportset.insert(
										NogoodContainer::createLiteral(
												reg->storeOrdinaryAtom(co)));
								supportset.insert(outlit);
								DBGLOG(DBG,
										"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
								potentialSupportSets->addNogood(supportset);
							} else if (rQID != ID_FAIL) {
								// guard atom for C(O1,O2)
								OrdinaryAtom co = theDLLitePlugin.getNewGuardAtom();
								co.tuple.push_back(cID);
								co.tuple.push_back(outvarID1);
								co.tuple.push_back(outvarID2);
								Nogood supportset;
								supportset.insert(NogoodContainer::createLiteral(reg->storeOrdinaryAtom(co)));
								supportset.insert(outlit);
								DBGLOG(DBG,"LSS: --> Learned support set: " << supportset.getStringRepresentation(reg));
								potentialSupportSets->addNogood(supportset);
							} else {
								assert(false);
							}
						}
					}
					en++;
				}
			}

			DBGLOG(DBG,"LSS: Number of learned nogoods: "<< potentialSupportSets->getNogoodCount());

		}
		optimizeSupportSets(potentialSupportSets, nogoods);

		DBGLOG(DBG, "LSS: finished support set learning");
	}

	void DLPluginAtom::optimizeSupportSets(SimpleNogoodContainerPtr initial,
			NogoodContainerPtr final) {

		DBGLOG(DBG, "EL: LSSO: Filter out irrelevant support sets");
		DBGLOG(DBG, "EL: LSSO: Abox predicates are:");
		std::vector<ID> abp;
		RegistryPtr reg = getRegistry();
		if (ctx.getPluginData<DLLitePlugin>().repair) {
			abp = theDLLitePlugin.prepareOntology(ctx,
					reg->storeConstantTerm(
							ctx.getPluginData<DLLitePlugin>().repairOntology))->AboxPredicates;
		} else if (ctx.getPluginData<DLLitePlugin>().rewrite) {
			abp = theDLLitePlugin.prepareOntology(ctx,
					reg->storeConstantTerm(
							ctx.getPluginData<DLLitePlugin>().ontology))->AboxPredicates;
		}
		BOOST_FOREACH(ID id,abp) {
			DBGLOG(DBG, RawPrinter::toString(reg,id));
		}

		int s = initial->getNogoodCount();
		//DBGLOG(DBG, "EL: LSSO: Inintial number of nonground support sets:"<<s);
		int n = 0;
		if (ctx.getPluginData<DLLitePlugin>().el) {
			for (int i = 0; i < s; i++) {
				bool elim = false;
				const Nogood& ng = initial->getNogood(i);
				//DBGLOG(DBG,"EL: consider support set: "<<ng.getStringRepresentation(reg));

				BOOST_FOREACH(ID litid, ng) {
					ID newid = litid;
					const OrdinaryAtom& oa = (litid.isOrdinaryGroundAtom() ? reg->ogatoms.getByID(litid) : reg->onatoms.getByID(litid));

					if ((newid.isGuardAuxiliary())&& (std::find(abp.begin(), abp.end(), oa.tuple[1])== abp.end())) {
						DBGLOG(DBG, "LSS: EL: "<< RawPrinter::toString(reg,newid)<<" is a guard predicate not occurring in ABox");
						elim = true;
						//	DBGLOG(DBG, "LSS: EL: support set is marked for elimination");
						break;
					} else {
						DBGLOG(DBG, "LSS: EL: "<<RawPrinter::toString(reg,newid)<<" occurs in the ABox");
					}
				}

				if (elim) {
					DBGLOG(DBG, "LSS: EL: eliminate current support set");
					initial->removeNogood(ng);
				} else {
					final->addNogood(ng);
					DBGLOG(DBG, "LSS: EL: leave current support set");
					n++;
				}
			}
			DBGLOG(DBG, "LSS: EL: Number of support sets after elimination: "<<n);
		}
		else {
			for (int i = 0; i < s; i++) {
				bool elim = false;
				const Nogood& ng = initial->getNogood(i);
				DBGLOG(DBG,"LSSO: consider support set: "<<ng.getStringRepresentation(reg));
				DBGLOG(DBG, "LSSO: is it of size >3? "<<ng.size());
				if (ng.size() > 3) {
					DBGLOG(DBG, "LSSO: yes");
					elim = true;
					DBGLOG(DBG, "LSSO: support set is marked for elimination");
				} else {
					DBGLOG(DBG, "LSSO: no");
					BOOST_FOREACH(ID litid, ng) {
						DBGLOG(DBG,"LSSO: consider element "<<RawPrinter::toString(reg,litid));

						const OrdinaryAtom& oa = reg->onatoms.getByAddress(litid.address);
						DBGLOG(DBG,"LSSO: is " <<RawPrinter::toString(reg,litid)<<" a guard with predicate not occurring in ABox?");
						DBGLOG(DBG,"LSSO: check for " <<RawPrinter::toString(reg,oa.tuple[1])<<" with "<<oa.tuple[1]);
						if ((litid.isGuardAuxiliary())&& (std::find(abp.begin(), abp.end(), oa.tuple[1])== abp.end())) {
							DBGLOG(DBG, "LSSO: yes");
							elim = true;
							DBGLOG(DBG, "LSSO: support set is marked for elimination");
							break;
						} else {
							DBGLOG(DBG, "LSSO: no");
						}
						DBGLOG(DBG,"LSSO: finished with "<<RawPrinter::toString(reg,litid));
					}
					DBGLOG(DBG,"LSSO: else block is finished");
				}
				DBGLOG(DBG,"LSSO: exit else block");
				if (elim) {
					DBGLOG(DBG, "LSSO: if current support set is marked, eliminate it");
					initial->removeNogood(ng);
				} else {
					final->addNogood(ng);
					DBGLOG(DBG, "LSSO: leave this support set");
					n++;
				}
			}
			DBGLOG(DBG, "LSSO: Number of support sets after elimination: "<<n);
		}

	}

	// ============================== Class CDLAtom ==============================

	CDLAtom::CDLAtom(ProgramCtx& ctx) :
	DLPluginAtom("cDL", ctx) {
		DBGLOG(DBG, "Constructor of cDL plugin is started");
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
	void CDLAtom::retrieve(const Query& query, Answer& answer,
			NogoodContainerPtr nogoods) {

		DBGLOG(DBG, "CDLAtom::retrieve");

		RegistryPtr reg = getRegistry();

		if (query.input.size() > 7)
		throw PluginError("cDL accepts at most 7 parameters");
		if (query.input.size() == 7
				&& (!query.input[6].isIntegerTerm() || query.input[6].address >= 2))
		throw PluginError("Last parameter of cDL must be 0 or 1");
		// TODO: add useAbox to other DL-atoms

		bool useAbox = !changeABox(query);

		//bool useAbox = !changeABox(query)&& (query.input.size() < 7 || query.input[6].address == 1);

		DBGLOG(DBG,"useABox = "<<useAbox);
		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0], useAbox);
		std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

		// handle inconsistency
		if (!ontology->kernel->isKBConsistent()) {
			// add all individuals to the output
			DBGLOG(DBG, "KB is inconsistent: returning all tuples");
			InterpretationPtr intr = ontology->getAllIndividuals(query);
			bm::bvector<>::enumerator en = intr->getStorage().first();
			bm::bvector<>::enumerator en_end = intr->getStorage().end();
			while (en < en_end) {
				Tuple tup;
				tup.push_back(
						ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));
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
		if (theDLLitePlugin.isDlNeg(query.input[5])) {
			negated = true;
			queryConceptID = theDLLitePlugin.dlNeg(queryConceptID);
		}
		std::string queryConcept = ontology->addNamespaceToString(
				reg->terms.getByID(queryConceptID).getUnquotedString());
		BOOST_FOREACH(owlcpp::Triple const& t, ontology->store.map_triple()) {
			DBGLOG(DBG,
					"Current triple: " << to_string(t.subj_, ontology->store) << " / " << to_string(t.pred_, ontology->store) << " / " << to_string(t.obj_, ontology->store));

			if (to_string(t.subj_, ontology->store) == queryConcept) {
				// found concept
				DBGLOG(DBG,
						"Preparing Actor_collector for " << to_string(t.subj_, ontology->store));
				Actor_collector ret(reg, answer, ontology,
						Actor_collector::Concept);
				DBGLOG(DBG, "Sending concept query");
				TDLConceptExpression* factppConcept =
				ontology->kernel->getExpressionManager()->Concept(
						to_string(t.subj_, ontology->store));

				if (negated)
				factppConcept = ontology->kernel->getExpressionManager()->Not(
						factppConcept);
				try {
					ontology->kernel->getInstances(factppConcept, ret);
				} catch (...) {
					throw PluginError(
							"DLLite reasoner failed during concept query");
				}
				found = true;
				break;
			}
		}
		if (!found) {
			DBGLOG(WARNING,
					"Queried non-existing concept " << ontology->removeNamespaceFromString(queryConcept));
		}

		DBGLOG(DBG, "Query answering complete, recovering Abox");
		restoreAbox(query, addedAxioms);
	}

	// ============================== Class RDLAtom ==============================

	RDLAtom::RDLAtom(ProgramCtx& ctx) :
	DLPluginAtom("rDL", ctx) {
		DBGLOG(DBG, "Constructor of rDL plugin is started");
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

	void RDLAtom::retrieve(const Query& query, Answer& answer,
			NogoodContainerPtr nogoods) {

		DBGLOG(DBG, "RDLAtom::retrieve");

		RegistryPtr reg = getRegistry();

		if (query.input.size() > 7)
		throw PluginError("rDL accepts at most 7 parameters");
		if (query.input.size() == 7
				&& (!query.input[6].isIntegerTerm() || query.input[6].address >= 2))
		throw PluginError("Last parameter of rDL must be 0 or 1");
		bool useAbox = !changeABox(query)
		&& (query.input.size() < 7 || query.input[6].address == 1);
		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0], useAbox);
		std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

		// handle inconsistency
		if (!ontology->kernel->isKBConsistent()) {
			// add all pairs of individuals to the output
			DBGLOG(DBG, "KB is inconsistent: returning all tuples");
			InterpretationPtr intr = ontology->getAllIndividuals(query);
			bm::bvector<>::enumerator en = intr->getStorage().first();
			bm::bvector<>::enumerator en_end = intr->getStorage().end();
			while (en < en_end) {
				bm::bvector<>::enumerator en2 = intr->getStorage().first();
				bm::bvector<>::enumerator en2_end = intr->getStorage().end();
				while (en2 < en2_end) {
					Tuple tup;
					tup.push_back(
							ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en));
					tup.push_back(
							ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT,
									*en2));
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
		if (theDLLitePlugin.isDlNeg(queryRoleID)) {
			queryRoleID = theDLLitePlugin.dlNeg(queryRoleID);
			negated = true;
			// TODO: Are such queries possible in DLLite?
			// I did not find a FaCT++ method for constructing negative role expressions
			throw PluginError("Negative role queries are not supported");
		}
		std::string role = reg->terms.getByID(queryRoleID).getUnquotedString();
		TDLObjectRoleExpression* factppRole =
		ontology->kernel->getExpressionManager()->ObjectRole(
				ontology->addNamespaceToString(role));
		DBGLOG(DBG, "Query is:" <<&factppRole);
		DBGLOG(DBG, "Answering role query");
		InterpretationPtr intr = ontology->getAllIndividuals(query);

		// for all individuals
		bm::bvector<>::enumerator en = intr->getStorage().first();
		bm::bvector<>::enumerator en_end = intr->getStorage().end();
		while (en < en_end) {
			ID individual = ID(ID::MAINKIND_TERM | ID::SUBKIND_TERM_CONSTANT, *en);

			// query related individuals
			DBGLOG(DBG,
					"Querying individuals related to " << RawPrinter::toString(reg, individual));
			std::vector<const TNamedEntry*> relatedIndividuals;
			try {
				DBGLOG(DBG, "Entered try block"<< ontology->reg->terms.getByID(individual).getUnquotedString());
				ontology->kernel->getRoleFillers(
						ontology->kernel->getExpressionManager()->Individual(
								ontology->addNamespaceToString(
										reg->terms.getByID(individual).getUnquotedString())),
						//			ontology->reg->terms.getByID(individual).getUnquotedString()),
						factppRole, relatedIndividuals);
			} catch (...) {
				throw PluginError("DLLite reasoner failed during role query");
			}

			// translate the result to HEX
			BOOST_FOREACH (const TNamedEntry* related, relatedIndividuals) {
				ID relatedIndividual = theDLLitePlugin.storeQuotedConstantTerm(
						ontology->removeNamespaceFromString(related->getName()));
				DBGLOG(DBG,
						"Adding role membership: (" << reg->terms.getByID(relatedIndividual).getUnquotedString() << ", " << relatedIndividual << ")");
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

	ConsDLAtom::ConsDLAtom(ProgramCtx& ctx) :
	DLPluginAtom("consDL", ctx, false)// consistency check is NOT monotonic!
	{
		DBGLOG(DBG, "Constructor of consDL plugin is started");
		addInputConstant(); // the ontology
		addInputPredicate(); // the positive concept
		addInputPredicate(); // the negative concept
		addInputPredicate(); // the positive role
		addInputPredicate(); // the negative role
		addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
		setOutputArity(0); // arity of the output list
	}

	void ConsDLAtom::retrieve(const Query& query, Answer& answer,
			NogoodContainerPtr nogoods) {

		DBGLOG(DBG, "ConsDLAtom::retrieve");

		RegistryPtr reg = getRegistry();

		if (query.input.size() > 6)
		throw PluginError("consDL accepts at most 6 parameters");
		if (query.input.size() == 6
				&& (!query.input[5].isIntegerTerm() || query.input[5].address >= 2))
		throw PluginError("Last parameter of consDL must be 0 or 1");
		bool useAbox = !changeABox(query)
		&& (query.input.size() < 6 || query.input[5].address == 1);
		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0], useAbox);
		std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

		// handle inconsistency
		if (ontology->kernel->isKBConsistent()) {
			answer.get().push_back(Tuple());
		}

		DBGLOG(DBG, "Consistency check complete, recovering Abox");
		restoreAbox(query, addedAxioms);
	}

	// ============================== Class InonsDLAtom ==============================

	InconsDLAtom::InconsDLAtom(ProgramCtx& ctx) :
	DLPluginAtom("inconsDL", ctx) {
		DBGLOG(DBG, "Constructor of inconsDL plugin is started");
		addInputConstant(); // the ontology
		addInputPredicate(); // the positive concept
		addInputPredicate(); // the negative concept
		addInputPredicate(); // the positive role
		addInputPredicate(); // the negative role
		addInputTuple(); // optional integer parameter: 0 to ignore the Abox from the ontology file, 1 to use it
		setOutputArity(0); // arity of the output list
	}

	void InconsDLAtom::retrieve(const Query& query, Answer& answer,
			NogoodContainerPtr nogoods) {

		DBGLOG(DBG, "InconsDLAtom::retrieve");

		RegistryPtr reg = getRegistry();

		if (query.input.size() > 6)
		throw PluginError("inconsDL accepts at most 6 parameters");
		if (query.input.size() == 6
				&& (!query.input[5].isIntegerTerm() || query.input[5].address >= 2))
		throw PluginError("Last parameter of inconsDL must be 0 or 1");

		bool useAbox = !changeABox(query)
		&& (query.input.size() < 6 || query.input[5].address == 1);
		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(
				ctx, query.input[0], useAbox);
		std::vector<TDLAxiom*> addedAxioms = expandAbox(query, useAbox);

		// handle inconsistency
		if (!ontology->kernel->isKBConsistent()) {
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


