/* dlvhex -- Answer-Set Programming with external interfaces.
 * Copyright (C) 2005, 2006, 2007 Roman Schindlauer
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Thomas Krennwallner
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
 * @file 	RepairModelGenerator.cpp
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
 *
 * @brief Implementation of the repair model generator for DLLite.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#undef DBGLOG
#define DBGLOG(level,msg) { std::cout << msg << std::endl; }

#include "RepairModelGenerator.h"
#include "dlvhex2/Logger.h"
#include "dlvhex2/Registry.h"
#include "dlvhex2/Printer.h"
#include "dlvhex2/ASPSolver.h"
#include "dlvhex2/ASPSolverManager.h"
#include "dlvhex2/ProgramCtx.h"
#include "dlvhex2/PluginInterface.h"
#include "dlvhex2/Benchmarking.h"
#include "dlvhex2/InternalGroundDASPSolver.h"
#include "dlvhex2/UnfoundedSetChecker.h"
#include "DLLitePlugin.h"
#include <bm/bmalgo.h>
#include <map>
#include <vector>
#include <boost/foreach.hpp>

DLVHEX_NAMESPACE_BEGIN

namespace dllite {
	extern DLLitePlugin theDLLitePlugin;

	RepairModelGeneratorFactory::RepairModelGeneratorFactory(
			ProgramCtx& ctx,
			const ComponentInfo& ci):
	FLPModelGeneratorFactoryBase(ctx),
	ctx(ctx),
	ci(ci),
	outerEatoms(ci.outerEatoms)
	{
		// this model generator can handle any components
		// (and there is quite some room for more optimization)

		// just copy all rules and constraints to idb
		idb.reserve(ci.innerRules.size() + ci.innerConstraints.size());
		idb.insert(idb.end(), ci.innerRules.begin(), ci.innerRules.end());
		idb.insert(idb.end(), ci.innerConstraints.begin(), ci.innerConstraints.end());

		// create program for domain exploration
		if (ctx.config.getOption("LiberalSafety")) {
			// add domain predicates for all external atoms which are necessary to establish liberal domain-expansion safety
			// and extract the domain-exploration program from the IDB
			addDomainPredicatesAndCreateDomainExplorationProgram(ci, ctx, idb, deidb, deidbInnerEatoms, outerEatoms);
		}

		innerEatoms = ci.innerEatoms;
		innerEatoms.insert(innerEatoms.end(), outerEatoms.begin(), outerEatoms.end());

		//allEatoms.insert(allEatoms.end(), innerEatoms.begin(), innerEatoms.end());
		//allEatoms.insert(allEatoms.end(), outerEatoms.begin(), outerEatoms.end());


		// create guessing rules "gidb" for innerEatoms in all inner rules and constraints
		createEatomGuessingRules(ctx);

		// transform original innerRules and innerConstraints to xidb with only auxiliaries
		xidb.reserve(idb.size());
		std::back_insert_iterator<std::vector<ID> > inserter(xidb);
		std::transform(idb.begin(), idb.end(),
				inserter, boost::bind(&RepairModelGeneratorFactory::convertRule, this, ctx, _1));

		// transform xidb for flp calculation
		if (ctx.config.getOption("FLPCheck")) createFLPRules();

		globalLearnedEANogoods = SimpleNogoodContainerPtr(new SimpleNogoodContainer());

		// output rules
		{
			std::ostringstream s;
			print(s, true);
			LOG(DBG,"RepairModelGeneratorFactory(): " << s.str());
		}
	}

	RepairModelGeneratorFactory::ModelGeneratorPtr
	RepairModelGeneratorFactory::createModelGenerator(
			InterpretationConstPtr input)
	{
		return ModelGeneratorPtr(new RepairModelGenerator(*this, input));
	}

	std::ostream& RepairModelGeneratorFactory::print(
			std::ostream& o) const
	{
		return print(o, true);
	}

	std::ostream& RepairModelGeneratorFactory::print(
			std::ostream& o, bool verbose) const
	{
		// item separator
		std::string isep(" ");
		// group separator
		std::string gsep(" ");
		if( verbose )
		{
			isep = "\n";
			gsep = "\n";
		}
		RawPrinter printer(o, ctx.registry());
		if( !outerEatoms.empty() )
		{
			o << "outer Eatoms={" << gsep;
			printer.printmany(outerEatoms,isep);
			o << gsep << "}" << gsep;
		}
		if( !innerEatoms.empty() )
		{
			o << "inner Eatoms={" << gsep;
			printer.printmany(innerEatoms,isep);
			o << gsep << "}" << gsep;
		}
		if( !gidb.empty() )
		{
			o << "gidb={" << gsep;
			printer.printmany(gidb,isep);
			o << gsep << "}" << gsep;
		}
		if( !idb.empty() )
		{
			o << "idb={" << gsep;
			printer.printmany(idb,isep);
			o << gsep << "}" << gsep;
		}
		if( !xidb.empty() )
		{
			o << "xidb={" << gsep;
			printer.printmany(xidb,isep);
			o << gsep << "}" << gsep;
		}
		if( !xidbflphead.empty() )
		{
			o << "xidbflphead={" << gsep;
			printer.printmany(xidbflphead,isep);
			o << gsep << "}" << gsep;
		}
		if( !xidbflpbody.empty() )
		{
			o << "xidbflpbody={" << gsep;
			printer.printmany(xidbflpbody,isep);
			o << gsep << "}" << gsep;
		}
		return o;
	}

	//
	// the model generator
	//

	RepairModelGenerator::RepairModelGenerator(
			Factory& factory,
			InterpretationConstPtr input):
	FLPModelGeneratorBase(factory, input),
	factory(factory),
	reg(factory.reg)
	{
		DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidconstruct, "Repair model generator constructor");
		DBGLOG(DBG, "RMG: Repair model generator is instantiated for a " << (factory.ci.disjunctiveHeads ? "" : "non-") << "disjunctive component");

		RegistryPtr reg = factory.reg;

		// create new interpretation as copy
		InterpretationPtr postprocInput;
		if( input == 0 )
		{
			// empty construction
			postprocInput.reset(new Interpretation(reg));
		}
		else
		{
			// copy construction
			postprocInput.reset(new Interpretation(*input));
		}

		// augment input with edb
#warning perhaps we can pass multiple partially preprocessed input edb's to the external solver and save a lot of processing here
		postprocInput->add(*factory.ctx.edb);

		// remember which facts we must remove
		mask.reset(new Interpretation(*postprocInput));

		// manage outer external atoms
		/*if( !factory.outerEatoms.empty() )
		 {
		 DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidhexground, "HEX grounder time");

		 // augment input with result of external atom evaluation
		 // use newint as input and as output interpretation
		 IntegrateExternalAnswerIntoInterpretationCB cb(postprocInput);
		 evaluateExternalAtoms(factory.ctx,
		 factory.outerEatoms, postprocInput, cb);
		 DLVHEX_BENCHMARK_REGISTER(sidcountexternalatomcomps,
		 "outer eatom computations");
		 DLVHEX_BENCHMARK_COUNT(sidcountexternalatomcomps,1);

		 //	We might still want to use G&C model generator even if it is not required (e.g. if support sets are used)
		 //      assert(!factory.xidb.empty() &&
		 //          "the guess and check model generator is not required for "
		 //          "non-idb components! (use plain)");
		 }*/

		// compute extensions of domain predicates and add it to the input


		if (factory.ctx.config.getOption("LiberalSafety")) {
			InterpretationConstPtr domPredicatesExtension = computeExtensionOfDomainPredicates(factory.ci, factory.ctx, postprocInput, factory.deidb, factory.deidbInnerEatoms);
			postprocInput->add(*domPredicatesExtension);
			DBGLOG(DBG,"RMG: after parsing the liberal safety option");
		}

		// assign to const member -> this value must stay the same from here on!
		postprocessedInput = postprocInput;

		// evaluate edb+xidb+gidb
		{
			DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"genuine g&c init guessprog");
			DBGLOG(DBG,"RMG: evaluating guessing program");
			// no mask
			OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
			// append gidb to xidb
			program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());

			{
				DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidhexground, "HEX grounder time");
				grounder = GenuineGrounder::getInstance(factory.ctx, program);
				//		annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.allEatoms);
				annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.innerEatoms);
			}
			solver = GenuineGroundSolver::getInstance(
					factory.ctx, annotatedGroundProgram,
					// no interleaved threading because guess and check MG will likely not profit from it
					InterpretationConstPtr(),
					// do the UFS check for disjunctions only if we don't do
					// a minimality check in this class;
					// this will not find unfounded sets due to external sources,
					// but at least unfounded sets due to disjunctions
					!factory.ctx.config.getOption("FLPCheck") && !factory.ctx.config.getOption("UFSCheck"));
			learnedEANogoods = SimpleNogoodContainerPtr(new SimpleNogoodContainer());
			learnedEANogoodsTransferredIndex = 0;
			nogoodGrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), learnedEANogoods, learnedEANogoods, annotatedGroundProgram));

			//if( factory.ctx.config.getOption("NoPropagator") == 0 )
			//  solver->addPropagator(this);
		}

		// setHeuristics();
		DBGLOG(DBG,"RMG: before calling learnSupportSets method");
		learnSupportSets();
		// DBGLOG(DBG,"RMG: learnSupportSets method is finished, "<<  supportSets->getNogoodCount() << " support sets were learnt");

		// initialize UFS checker
		//   Concerning the last parameter, note that clasp backend uses choice rules for implementing disjunctions:
		//   this must be regarded in UFS checking (see examples/trickyufs.hex)

		ufscm = UnfoundedSetCheckerManagerPtr(new UnfoundedSetCheckerManager(*this, factory.ctx, annotatedGroundProgram, factory.ctx.config.getOption("GenuineSolver") >= 3, factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr()));
		// overtake nogoods from the factory
		{
			DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"genuine g&c init nogoods");
			for (int i = 0; i < factory.globalLearnedEANogoods->getNogoodCount(); ++i) {
				DBGLOG(DBG,"RMG: going through nogoods");
				DBGLOG(DBG,"RMG: consider nogood number "<< i<< " which is "<< factory.globalLearnedEANogoods->getNogood(i));
				learnedEANogoods->addNogood(factory.globalLearnedEANogoods->getNogood(i));
			}
			updateEANogoods();
		}
		DBGLOG(DBG,"RMG: RepairModelGenerator constructor is finished");
	}

	RepairModelGenerator::~RepairModelGenerator() {
		//solver->removePropagator(this)
		DBGLOG(DBG, "Final Statistics:" << std::endl << solver->getStatistics());
	}

	//called from the core
	InterpretationPtr RepairModelGenerator::generateNextModel() {
		// now we have postprocessed input in postprocessedInput
		DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidgcsolve, "genuine guess and check loop");
		DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidhexsolve, "HEX solver time");

		InterpretationPtr modelCandidate;
		do
		{
			LOG(DBG,"asking for next model");
			modelCandidate = solver->getNextModel();
			//	DBGLOG(DBG,"a model candidate is obtained: " << *modelCandidate);
			// getnextmodel calls propogate method
			// model is returned
			DBGLOG(DBG, "Statistics:" << std::endl << solver->getStatistics());
			if( !modelCandidate )
			{
				LOG(DBG,"unsatisfiable -> returning no model");
				return InterpretationPtr();
			}
			DLVHEX_BENCHMARK_REGISTER_AND_COUNT(ssidmodelcandidates, "Candidate compatible sets", 1);
			LOG_SCOPE(DBG,"gM", false);

			LOG(DBG,"got guess model " << *modelCandidate);

			if (postCheck(modelCandidate)) {
				return modelCandidate;
			}

			//go through elements of the model candidate and


			//LOG(DBG,"got guess model, will do repair check on " << *modelCandidate);
			/*if (!repairCheck(modelCandidate))
			 {
			 LOG(DBG,"RMG: no repair ABox was found");
			 continue;
			 //	return InterpretationPtr();

			 }
			 else {
			 LOG(DBG,"RMG: repair ABox was found");

			 modelCandidate->getStorage() -= factory.gpMask.mask()->getStorage();
			 modelCandidate->getStorage() -= factory.gnMask.mask()->getStorage();
			 modelCandidate->getStorage() -= mask->getStorage();

			 LOG(DBG,"returning model without guess: " << *modelCandidate);
			 return modelCandidate;
			 }*/

			/*else {
			 DBGLOG(DBG, "Checking if model candidate is a model");
			 if (!isModel(modelCandidate))
			 {
			 LOG(DBG,"Model candidate is not a model (isModel failed)");
			 continue;
			 }


			 // remove edb and the guess (from here we don't need the guess anymore)
			 modelCandidate->getStorage() -= factory.gpMask.mask()->getStorage();
			 modelCandidate->getStorage() -= factory.gnMask.mask()->getStorage();
			 modelCandidate->getStorage() -= mask->getStorage();

			 LOG(DBG,"returning model without guess: " << *modelCandidate);
			 return modelCandidate;}*/
		}while(true);
	}

	void RepairModelGenerator::generalizeNogood(Nogood ng) {

	}

	void RepairModelGenerator::learnSupportSets() {
		DBGLOG(DBG,"EL: RMG: learning support sets is started");
		DBGLOG(DBG,"EL: RMG: Number of all eatoms: "<<factory.innerEatoms.size());
		std::vector<SimpleNogoodContainerPtr> supportSetsOfExternalAtom;
		std::vector<ID> complext;

		if (factory.ctx.config.getOption("SupportSets")) {
			OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
			program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());
			if (factory.ctx.getPluginData<DLLitePlugin>().el) {
				//OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
				//program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());

				// information about limit of the support sets:
				if (factory.ctx.getPluginData<DLLitePlugin>().supsize!=-1) {
					DBGLOG(DBG,"EL: RMG: support set size limit is: "<<factory.ctx.getPluginData<DLLitePlugin>().supsize);
				}
				//else {
				//	DBGLOG(DBG,"EL: RMG: there is no limit on support set size");
				//}
				if (factory.ctx.getPluginData<DLLitePlugin>().supnumber!=-1)
				DBGLOG(DBG,"EL: RMG: support set number limit is: "<<factory.ctx.getPluginData<DLLitePlugin>().supnumber);
				//else {
				//	DBGLOG(DBG,"EL: RMG: there is no limit on the support set number");
				//}

				if (factory.ctx.getPluginData<DLLitePlugin>().incomplete)
				DBGLOG(DBG,"EL: RMG: no support family is known to be complete");
				//else {
				//	DBGLOG(DBG,"EL: all support families are complete");
				//}

				ID guardPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 0));
				ID guardbarPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 1));
				ID evalPredicateID = reg->getAuxiliaryConstantSymbol('e', ID(0, 0));
				ID complPredicateID = reg->getAuxiliaryConstantSymbol('c', ID(0, 0));
				ID varoID1 = reg->storeVariableTerm("O0");
				ID varoID2 = reg->storeVariableTerm("O1	");

				DBGLOG(DBG, "EL: RMG: program idb before support set learning and additional rule construction: ");
				for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
					DBGLOG(DBG, "EL: RMG: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
				}

				for(unsigned eaIndex = 0; eaIndex < factory.innerEatoms.size(); ++eaIndex) {
					DBGLOG(DBG,"EL: RMG: consider atom "<< RawPrinter::toString(reg,factory.innerEatoms[eaIndex]));
					const ExternalAtom& eatom = reg->eatoms.getByID(factory.innerEatoms[eaIndex]);

					supportSetsOfExternalAtom.push_back(SimpleNogoodContainerPtr(new SimpleNogoodContainer()));
					if (eatom.getExtSourceProperties().providesSupportSets()) {
						//DBGLOG(DBG, "RMG: evaluating external atom " << RawPrinter::toString(reg,factory.allEatoms[eaIndex]) << " for support set learning");
						DBGLOG(DBG, "EL: RMG: evaluating external atom " << RawPrinter::toString(reg,factory.innerEatoms[eaIndex]) << " for support set learning");
						learnSupportSetsForExternalAtom(factory.ctx, eatom, supportSetsOfExternalAtom[eaIndex]);
						DBGLOG(DBG, "EL: RMG: number of learnt support sets: "<<supportSetsOfExternalAtom[eaIndex]->getNogoodCount());
					}

					// prepare for rewriting
					// cQID is the predicate for concept query
					// rQID is the predicate for role query

					ID cQID = ID_FAIL;
					ID rQID = ID_FAIL;

					ID cdlID = reg->storeConstantTerm("cDL");
					ID rdlID = reg->storeConstantTerm("rDL");

					if (eatom.predicate==cdlID) {
						// query is a concept
						cQID = eatom.inputs[5];
					}

					else if (eatom.predicate==rdlID) {
						//query is a role
						rQID = eatom.inputs[5];
					}

					else assert(false);

					// s is number of nogoods for considered external atom (with index eaIndex)
					int s = supportSetsOfExternalAtom[eaIndex]->getNogoodCount();
					int supsizelimit = factory.ctx.getPluginData<DLLitePlugin>().supsize;
					int supnumberlimit = factory.ctx.getPluginData<DLLitePlugin>().supnumber;
					bool incomplete = factory.ctx.getPluginData<DLLitePlugin>().incomplete;

					// go through nogoods for current external atom
					for (int i = 0; i < s; i++) {
						const Nogood& ng = supportSetsOfExternalAtom[eaIndex]->getNogood(i);
						if ((supnumberlimit!=-1)&&(i>supnumberlimit)) {
							DBGLOG(DBG, "EL: RMG: the limit on number of support sets is reached ");
							complext.push_back(factory.innerEatoms[eaIndex]);
							break;
						}

						if ((supsizelimit!=-1)&&(ng.size()>supsizelimit+1)) {
							DBGLOG(DBG, "EL: RMG: skip support set, its size exceeds the limit "<<supsizelimit);
						}

						else {
							DBGLOG(DBG, "EL: RMG: creating rules from support set " << ng.getStringRepresentation(reg));

							// create guard atom that will be used for rules
							OrdinaryAtom guard(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
							guard.tuple.push_back(guardPredicateID);
							guard.tuple.push_back(eatom.tuple[5]);

							// create variable for identifying whether guard is present in support set
							ID guardID = ID_FAIL;

							// create two vectors for storing ontology and logic program parts
							std::vector<ID> ontopart;
							std::vector<ID> progpart;

							// go through literals of nogood and for guards identify their ontology predicate

							BOOST_FOREACH (ID id, ng) {
								const OrdinaryAtom& oatom = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByAddress(id.address) : reg->onatoms.getByAddress(id.address));
								if (oatom.tuple[0] == guardPredicateID) {
									ontopart.push_back(id);
								}

								else if ((oatom.tuple[0] == eatom.inputs[1])||(oatom.tuple[0] == eatom.inputs[3])) {
									progpart.push_back(id);
								}
							}

							if (ontopart.size()!=0) {
								// case: ontology part of the support set is nonempty


								// case: program part of the support set is nonempty

								DBGLOG(DBG, "EL: RMG: ontology part is nonempty ");

								{	DBGLOG(DBG,"EL: RMG: RULE: bar_aux_o(P_1,...) v bar_aux_o(P_n, ...):-aux_p(D,Y), aux_o(P_1,...), aux_o(P_n,...), n_e_a(Q,O). (neg. repl. of eatom)");

									Rule choosingRule(ID::MAINKIND_RULE | ID::PROPERTY_RULE_DISJ);
									//		DBGLOG(DBG, "EL: RMG: disjunctive rule is created");
									// HEAD: bar_aux_o("P_1",...) v...v bar_aux_o("P_n",...):-
									// BODY: aux_o("P_1",...),bar_zux_o("P_n",...)
									{
										std::vector<ID>::iterator it = ontopart.begin();
										std::vector<ID>::iterator it_end = ontopart.end();
										while (it < it_end) {
											//	DBGLOG(DBG, "EL: RMG: go through ontology part" );
											ID id = *it;
											OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (id.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
											headat.tuple.push_back(guardbarPredicateID);
											headat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[1]);
											if (reg->lookupOrdinaryAtom(id).tuple.size()==3) {
												headat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(id).tuple.size()==4) {
												headat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
												headat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[3]);
											}

											OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (id.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
											bodyat.tuple.push_back(guardPredicateID);
											bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[1]);
											if (reg->lookupOrdinaryAtom(id).tuple.size()==3) {
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(id).tuple.size()==4) {
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[3]);
											}
											choosingRule.head.push_back(reg->storeOrdinaryAtom(headat));
											choosingRule.body.push_back(reg->storeOrdinaryAtom(bodyat));
											//	DBGLOG(DBG, "EL: RMG: went through ontology element "<< RawPrinter::toString(reg, id) );

											it++;
										}
									}

									// BODY: n_e_a(Q,O)
									{	OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}

										choosingRule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										//DBGLOG(DBG, "EL: RMG: added negative replacement atom" );

									}

									if (progpart.size()!=0) {
										// BODY: logic program part
										{
											std::vector<ID>::iterator it = progpart.begin();
											std::vector<ID>::iterator it_end = progpart.end();
											//	DBGLOG(DBG, "EL: RMG: start going through logic program part" );

											while (it < it_end) {
												//	DBGLOG(DBG, "EL: RMG: current logic program predicate is  "<< RawPrinter::toString(reg,*it));
												ID id=*it;
												ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));

												// add logic program predicates to the body
												choosingRule.body.push_back(ID::posLiteralFromAtom(idOrig));
												//	DBGLOG(DBG, "EL: RMG: added a predicate to the body ");

												it++;
											}
										}

									}

									ID choosingRuleID = reg->storeRule(choosingRule);
									DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, choosingRuleID));
									program.idb.push_back(choosingRuleID);

									DBGLOG(DBG, "EL: RMG: RULE: IDB:  program idb after rule addition: ");
									for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
										DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
									}

								}

								{
									DBGLOG(DBG,"EL: RMG: RULE:  supp_e_a(Q,O):-aux_p(D,Y), aux_o(C,X),e_a(Q,O), not bar_aux_o(C,X).");
									Rule rule(ID::MAINKIND_RULE);
									//	DBGLOG(DBG, "EL: RMG: rule is created");

									// HEAD: supp_e_a("Q",O)
									{
										OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
										headat.tuple.push_back(eatom.inputs[0]);
										headat.tuple.push_back(eatom.inputs[1]);
										headat.tuple.push_back(eatom.inputs[2]);
										headat.tuple.push_back(eatom.inputs[3]);
										headat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											headat.tuple.push_back(cQID);
											headat.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											headat.tuple.push_back(rQID);
											headat.tuple.push_back(varoID1);
											headat.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.head.push_back(reg->storeOrdinaryAtom(headat));
									}

									//BODY: not bar_aux_o(P,...), ...not bar_aux_o(P_n,...), aux_o(P_n,...), ...aux_o(P_n,...).
									{
										std::vector<ID>::iterator it = ontopart.begin();
										std::vector<ID>::iterator it_end = ontopart.end();
										while (it < it_end) {
											//	DBGLOG(DBG, "EL: RMG: go through ontology part" );
											ID id = *it;
											OrdinaryAtom bodyatbar(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (id.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));

											bodyatbar.tuple.push_back(guardbarPredicateID);
											bodyatbar.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[1]);

											if (reg->lookupOrdinaryAtom(id).tuple.size()==3) {
												bodyatbar.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(id).tuple.size()==4) {
												bodyatbar.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
												bodyatbar.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[3]);
											}

											OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (id.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
											bodyat.tuple.push_back(guardPredicateID);
											bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[1]);

											if (reg->lookupOrdinaryAtom(id).tuple.size()==3) {
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(id).tuple.size()==4) {
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[2]);
												bodyat.tuple.push_back(reg->lookupOrdinaryAtom(id).tuple[3]);
											}

											rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(bodyatbar)));
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(bodyat)));
											//		DBGLOG(DBG, "EL: RMG: went through ontology element "<< RawPrinter::toString(reg, id) );

											it++;

										}
									}

									// BODY: e_a(Q,O)

									{	OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										//DBGLOG(DBG, "EL: RMG: added negative replacement atom" );
									}

									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);

									DBGLOG(DBG, "EL: RMG: RULE: IDB: program idb after rule addition: ");
									for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
										DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
									}
								}

								if (!incomplete) {

									DBGLOG(DBG, "EL: RMG: since support familiare complete, we add");
									DBGLOG(DBG, "EL: RMG: RULE: :-e_a(Q,O),not supp_e_a(Q,O)");
									//   RULE   :-e_a("Q",O),not supp_e_a("Q",O).
									{
										Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID1);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										}
										{
											OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											//	notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
											notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
											// distinct
											notsupp.tuple.push_back(eatom.inputs[0]);
											notsupp.tuple.push_back(eatom.inputs[1]);
											notsupp.tuple.push_back(eatom.inputs[2]);
											notsupp.tuple.push_back(eatom.inputs[3]);
											notsupp.tuple.push_back(eatom.inputs[4]);

											if (cQID!=ID_FAIL) {
												notsupp.tuple.push_back(cQID);
												notsupp.tuple.push_back(varoID1);
											}
											else if (rQID!=ID_FAIL) {
												notsupp.tuple.push_back(rQID);
												notsupp.tuple.push_back(varoID1);
												notsupp.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
										}
										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);

										DBGLOG(DBG, "EL: RMG: RULE: IDB: program idb after rule addition: \n");
										for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
											DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
										}
									}
								}

							}

							else {
								DBGLOG(DBG, "EL: RMG: ontology part is empty ");
								{	DBGLOG(DBG,"EL: RMG: RULE: :- n_e_a(Q,O), aux_p(P,X)");

									Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
									//	DBGLOG(DBG, "EL: RMG: rule is created");

									// BODY: n_e_a(Q,O)
									{	OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										//	DBGLOG(DBG, "EL: RMG: added negative replacement atom" );

									}
									//BODY: aux_p(P,X)
									{
										std::vector<ID>::iterator it = progpart.begin();
										std::vector<ID>::iterator it_end = progpart.end();
										//DBGLOG(DBG, "EL: RMG: start going through logic program part" );

										while (it < it_end) {
											//	DBGLOG(DBG, "EL: RMG: current logic program predicate is  "<< RawPrinter::toString(reg,*it));
											ID id=*it;
											ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));
											// add logic program predicates to the body
											rule.body.push_back(ID::posLiteralFromAtom(idOrig));
											//		DBGLOG(DBG, "EL: RMG: added a predicate to the body ");
											it++;
										}
									}

									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);

									DBGLOG(DBG, "EL: RMG: RULE: program idb after rule addition: \n");
									for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
										DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
									}
								}

							}
							if (incomplete) {
								{

									DBGLOG(DBG, "EL: RMG: support families are incomplete, thus we add two rule with eval in heads");
									DBGLOG(DBG,"EL: RMG: RULE:  eval_e_a(Q,O):- not supp_e_a(Q,O),e_a(Q,O), not comp_e_a(Q,O).");
									Rule rule(ID::MAINKIND_RULE);
									//	DBGLOG(DBG, "EL: RMG: rule is created");

									{
										//HEAD: eval_e_a(Q,O)
										OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
										bodyat.tuple.push_back(evalPredicateID);
										bodyat.tuple.push_back(eatom.inputs[0]);
										bodyat.tuple.push_back(eatom.inputs[1]);
										bodyat.tuple.push_back(eatom.inputs[2]);
										bodyat.tuple.push_back(eatom.inputs[3]);
										bodyat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											bodyat.tuple.push_back(cQID);
											bodyat.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											bodyat.tuple.push_back(rQID);
											bodyat.tuple.push_back(varoID1);
											bodyat.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.head.push_back(reg->storeOrdinaryAtom(bodyat));

									}

									{
										//BODY: supp_e_a(Q,O)
										OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										bodyat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
										bodyat.tuple.push_back(eatom.inputs[0]);
										bodyat.tuple.push_back(eatom.inputs[1]);
										bodyat.tuple.push_back(eatom.inputs[2]);
										bodyat.tuple.push_back(eatom.inputs[3]);
										bodyat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											bodyat.tuple.push_back(cQID);
											bodyat.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											bodyat.tuple.push_back(rQID);
											bodyat.tuple.push_back(varoID1);
											bodyat.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(bodyat)));
									}

									{
										//BODY: e_a(Q,O)
										OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));

									}

									{
										//BODY: not comp_e_a(Q,O)
										OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
										bodyat.tuple.push_back(complPredicateID);
										bodyat.tuple.push_back(eatom.inputs[0]);
										bodyat.tuple.push_back(eatom.inputs[1]);
										bodyat.tuple.push_back(eatom.inputs[2]);
										bodyat.tuple.push_back(eatom.inputs[3]);
										bodyat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											bodyat.tuple.push_back(cQID);
										}
										else if (rQID!=ID_FAIL) {
											bodyat.tuple.push_back(rQID);
										} else {assert(false);}
										rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(bodyat)));

									}

									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);

									DBGLOG(DBG, "EL: RMG: RULE: program idb after rule addition: \n");
									for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
										DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
									}

								}

								{
									DBGLOG(DBG,"EL: RMG: RULE:  eval_e_a(Q,O):- not supp_e_a(Q,O),e_a(Q,O), not comp_e_a(Q,O).");
									Rule rule(ID::MAINKIND_RULE);
									//	DBGLOG(DBG, "EL: RMG: rule is created");

									{
										//HEAD: eval_e_a(Q,O)
										OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
										headat.tuple.push_back(evalPredicateID);
										headat.tuple.push_back(eatom.inputs[0]);
										headat.tuple.push_back(eatom.inputs[1]);
										headat.tuple.push_back(eatom.inputs[2]);
										headat.tuple.push_back(eatom.inputs[3]);
										headat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											headat.tuple.push_back(cQID);
											headat.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											headat.tuple.push_back(rQID);
											headat.tuple.push_back(varoID1);
											headat.tuple.push_back(varoID2);

										} else {assert(false);}

										rule.head.push_back(reg->storeOrdinaryAtom(headat));
									}

									{
										OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										DBGLOG(DBG, "EL: RMG: added negative replacement atom" );
									}

									{
										//BODY: not comp_e_a(Q)
										OrdinaryAtom bodyat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
										bodyat.tuple.push_back(complPredicateID);
										bodyat.tuple.push_back(eatom.inputs[0]);
										bodyat.tuple.push_back(eatom.inputs[1]);
										bodyat.tuple.push_back(eatom.inputs[2]);
										bodyat.tuple.push_back(eatom.inputs[3]);
										bodyat.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											bodyat.tuple.push_back(cQID);
											//bodyat.tuple.push_back(varoID1);
										}
										else if (rQID!=ID_FAIL) {
											bodyat.tuple.push_back(rQID);
											//bodyat.tuple.push_back(varoID1);
											//bodyat.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(bodyat)));

									}
									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "EL: RMG: RULE: adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);

									DBGLOG(DBG, "EL: RMG: RULE: program idb after rule addition: \n");
									for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
										DBGLOG(DBG, "EL: RMG: RULE: IDB: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
									}
								}

							}
						}

					}

				}

				DBGLOG(DBG, "EL: RMG: adding Abox");
				InterpretationPtr edb(new Interpretation(reg));
				edb->add(*program.edb);
				//DBGLOG(DBG, "EL: RMG: program edb before adding ABox "<<*edb);
				program.edb = edb;
				DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(factory.ctx, reg->storeConstantTerm(factory.ctx.getPluginData<DLLitePlugin>().repairOntology));

				// add ontology ABox in form of facts aux_o("D",c)
				edb->add(*ontology->conceptAssertions);

				// (accordingly aux_o("R",c1,c2)).
				BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion ra, ontology->roleAssertions) {
					OrdinaryAtom roleAssertion(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
					roleAssertion.tuple.push_back(guardPredicateID);
					roleAssertion.tuple.push_back(ra.first);
					roleAssertion.tuple.push_back(ra.second.first);
					roleAssertion.tuple.push_back(ra.second.second);
					edb->setFact(reg->storeOrdinaryAtom(roleAssertion).address);
				}
				DBGLOG(DBG, "EL: RMG: program edb after adding ABox "<<*edb);

				DBGLOG(DBG, "EL: RMG: adding information about support set completeness ");


				for(unsigned eaIndex = 0; eaIndex < factory.innerEatoms.size(); ++eaIndex) {
					//	DBGLOG(DBG,"EL: RMG: adding completeness fact for atom "<< RawPrinter::toString(reg,factory.innerEatoms[eaIndex]));
					const ExternalAtom& eatom = reg->eatoms.getByID(factory.innerEatoms[eaIndex]);
					ID cQID = ID_FAIL;
					ID rQID = ID_FAIL;

					ID cdlID = reg->storeConstantTerm("cDL");
					ID rdlID = reg->storeConstantTerm("rDL");

					if (eatom.predicate==cdlID) {
						// query is a concept
						cQID = eatom.inputs[5];
					}

					else if (eatom.predicate==rdlID) {
						//query is a role
						rQID = eatom.inputs[5];
					}

					else assert(false);

					if (!factory.ctx.getPluginData<DLLitePlugin>().incomplete) {

						DBGLOG(DBG, "EL: RMG: incomplete=false, i.e. support families are complete, add comp predicates ");

						{
							OrdinaryAtom comp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
							comp.tuple.push_back(reg->getAuxiliaryConstantSymbol('c', factory.innerEatoms[eaIndex]));
							comp.tuple.push_back(eatom.inputs[0]);
							comp.tuple.push_back(eatom.inputs[1]);
							comp.tuple.push_back(eatom.inputs[2]);
							comp.tuple.push_back(eatom.inputs[3]);
							comp.tuple.push_back(eatom.inputs[4]);
							comp.tuple.push_back(eatom.inputs[5]);
							if (cQID!=ID_FAIL)
							comp.tuple.push_back(cQID);
							else if (rQID!=ID_FAIL)
							comp.tuple.push_back(rQID);
							else {assert(false);}
							edb->setFact(reg->storeOrdinaryAtom(comp).address);
						}
					}

					else if (factory.ctx.getPluginData<DLLitePlugin>().supnumber!=-1) {
						if (std::find(complext.begin(), complext.end(), factory.innerEatoms[eaIndex]) != complext.end()){
							{
								OrdinaryAtom comp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
								comp.tuple.push_back(reg->getAuxiliaryConstantSymbol('c', factory.innerEatoms[eaIndex]));
								comp.tuple.push_back(eatom.inputs[0]);
								comp.tuple.push_back(eatom.inputs[1]);
								comp.tuple.push_back(eatom.inputs[2]);
								comp.tuple.push_back(eatom.inputs[3]);
								comp.tuple.push_back(eatom.inputs[4]);
								comp.tuple.push_back(eatom.inputs[5]);
								if (cQID!=ID_FAIL)
								comp.tuple.push_back(cQID);
								else if (rQID!=ID_FAIL)
								comp.tuple.push_back(rQID);
								else {assert(false);}
								edb->setFact(reg->storeOrdinaryAtom(comp).address);
							}
						}
					}
				}

				DBGLOG(DBG, "EL: RMG: Program edb after adding completeness predicates"<<*edb);

				bm::bvector<>::enumerator enm;
				bm::bvector<>::enumerator enm_end;
				enm = edb->getStorage().first();
				enm_end = edb->getStorage().end();

				DBGLOG(DBG, "EL: RMG: program idb: ");
				for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
					DBGLOG(DBG, "EL: RMG: "<<RawPrinter::toString(reg,program.idb[ruleIndex])<<"\n");
				}

				// ground the program and evaluate it
				// get the results, filter them out with respect to only relevant predicates (all apart from aux_o, replacement atoms)
				grounder = GenuineGrounder::getInstance(factory.ctx, program);
				//	DBGLOG(DBG, "EL: RMG: after grounding");
				// annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.allEatoms);
				annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.innerEatoms);
				DBGLOG(DBG, "EL: RMG: annotated ground program is constructed");
				solver = GenuineGroundSolver::getInstance(
						factory.ctx, annotatedGroundProgram,
						InterpretationConstPtr(),
						!factory.ctx.config.getOption("FLPCheck") && !factory.ctx.config.getOption("UFSCheck"));
				//DBGLOG(DBG, "EL: RMG: after calling the solver");
				nogoodGrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), learnedEANogoods, learnedEANogoods, annotatedGroundProgram));
				//DBGLOG(DBG, "EL: RMG: after creating a nogood grounder");

			}
			else {

				OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
				program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());

				ID guardPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 0));
				ID guardbarPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 1));

				ID varoID = reg->storeVariableTerm("O");
				ID varoID1 = reg->storeVariableTerm("O1");
				ID varoID2 = reg->storeVariableTerm("O2");

				for(unsigned eaIndex = 0; eaIndex < factory.innerEatoms.size(); ++eaIndex) {

					// DBGLOG(DBG,"RMG: consider atom "<< RawPrinter::toString(reg,factory.allEatoms[eaIndex]));
					DBGLOG(DBG,"RMG: consider atom "<< RawPrinter::toString(reg,factory.innerEatoms[eaIndex]));

					// evaluate the external atom if it provides support sets

					// const ExternalAtom& eatom = reg->eatoms.getByID(factory.allEatoms[eaIndex]);
					const ExternalAtom& eatom = reg->eatoms.getByID(factory.innerEatoms[eaIndex]);

					supportSetsOfExternalAtom.push_back(SimpleNogoodContainerPtr(new SimpleNogoodContainer()));
					if (eatom.getExtSourceProperties().providesSupportSets()) {
						// DBGLOG(DBG, "RMG: evaluating external atom " << RawPrinter::toString(reg,factory.allEatoms[eaIndex]) << " for support set learning");
						DBGLOG(DBG, "RMG: evaluating external atom " << RawPrinter::toString(reg,factory.innerEatoms[eaIndex]) << " for support set learning");
						learnSupportSetsForExternalAtom(factory.ctx, eatom, supportSetsOfExternalAtom[eaIndex]);
						DBGLOG(DBG, "RMG: number of learnt support sets: "<<supportSetsOfExternalAtom[eaIndex]->getNogoodCount());
					}

					// prepare for rewriting
					// cQID is the predicate for concept query
					// rQID is the predicate for role query

					ID cQID = ID_FAIL;
					ID rQID = ID_FAIL;

					ID cdlID = reg->storeConstantTerm("cDL");
					ID rdlID = reg->storeConstantTerm("rDL");

					if (eatom.predicate==cdlID) {
						cQID = eatom.inputs[5];
					}

					else if (eatom.predicate==rdlID) {
						rQID = eatom.inputs[5];
					} else assert(false);

					// s is number of nogoods for considered external atom (with index eaIndex)
					int s = supportSetsOfExternalAtom[eaIndex]->getNogoodCount();

					// go through nogoods for current external atom
					for (int i = 0; i < s; i++) {
						const Nogood& ng = supportSetsOfExternalAtom[eaIndex]->getNogood(i);
						DBGLOG(DBG, "RMG: checking support set " << ng.getStringRepresentation(reg));

						// create guard atom that will be used for rules
						OrdinaryAtom guard(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
						guard.tuple.push_back(guardPredicateID);
						guard.tuple.push_back(eatom.tuple[5]);

						// create variables for identifying whether guard is present in support set and for distincting role and concept guards
						ID guardID = ID_FAIL;
						ID cID = ID_FAIL;

						// go through literals of nogood and for guards identify their ontology predicate
						BOOST_FOREACH (ID id, ng) {
							const OrdinaryAtom& oatom = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByAddress(id.address) : reg->onatoms.getByAddress(id.address));
							if (oatom.tuple[0] == guardPredicateID) {
								guardID = id;
								cID = oatom.tuple[1];
								break;
							}
						}

						bool foundOrdinaryAtom = false;
						if (guardID != ID_FAIL) {

							// case: support set has a guard
							ID guardIDOrig = (guardID.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(guardID.address) : reg->onatoms.getIDByAddress(guardID.address));

							BOOST_FOREACH (ID id, ng) {

								// restore flags of ID
								ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));
								DBGLOG(DBG, "RMG: checking literal " << RawPrinter::toString(reg, idOrig));

								//keep current ordinary atom of ng in oatom
								const OrdinaryAtom& oatom = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByAddress(id.address) : reg->onatoms.getByAddress(id.address));

								// check if it is a c+ atom
								if (oatom.tuple[0] == eatom.inputs[1]) {

									// case: support set has both a guard and normal update atom
									// current atom is a normal atom aux_p("D",Y)) {


									DBGLOG(DBG, "RMG: literal is a concept");
									// create rule:
									// * bar_aux_o("C",X):-aux_p("D",Y), aux_o("C",X), n_e_a("Q",O). (neg. repl. of eatom)

									{	DBGLOG(DBG,"RMG: RULE: bar_aux_o(C,X):-aux_p(D,Y), aux_o(C,X), n_e_a(Q,O). (neg. repl. of eatom)");

										Rule rule(ID::MAINKIND_RULE);

										// HEAD: bar_aux_o("C",X)
										{
											OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
											headat.tuple.push_back(guardbarPredicateID);
											//distinct between concep and role guards
											headat.tuple.push_back(cID);
											if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
												headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
												headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
												headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
											} else assert(false);

											rule.head.push_back(reg->storeOrdinaryAtom(headat));
										}

										// BODY: aux_p(D,Y)
										rule.body.push_back(ID::posLiteralFromAtom(idOrig));

										// BODY: aux_o("C",X)
										rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));

										// BODY: n_e_a(Q,O)
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										}

										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}

									DBGLOG(DBG,"RMG: RULE: supp_e_a(Q,O):-aux_p(D,Y), aux_o(C,X),e_a(Q,O), not bar_aux_o(C,X).");

									{
										Rule rule(ID::MAINKIND_RULE);
										// HEAD: supp_e_a("Q",O)
										{
											OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											// headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
											headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));

											headat.tuple.push_back(eatom.inputs[0]);
											headat.tuple.push_back(eatom.inputs[1]);
											headat.tuple.push_back(eatom.inputs[2]);
											headat.tuple.push_back(eatom.inputs[3]);
											headat.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {

												headat.tuple.push_back(cQID);
												headat.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												headat.tuple.push_back(rQID);
												headat.tuple.push_back(varoID1);
												headat.tuple.push_back(varoID2);
											} else {assert(false);}

											rule.head.push_back(reg->storeOrdinaryAtom(headat));
										}

										// BODY: aux_p("D",Y)
										rule.body.push_back(ID::posLiteralFromAtom(idOrig));

										// BODY: aux_o("C",X)
										rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));

										// BODY: e_a("Q",O)
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);

											// distinct between concept and role queries
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else assert(false);

											rule.body.push_back(reg->storeOrdinaryAtom(repl));
										}

										// BODY: not bar_aux_o("C",X)
										{
											OrdinaryAtom notbarat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											notbarat.tuple.push_back(guardbarPredicateID);
											//distinct between concep and role guards
											notbarat.tuple.push_back(cID);
											if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
												notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											}
											else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
												notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
												notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
											} else assert(false);

											rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notbarat)));
										}

										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}

									DBGLOG(DBG, "RMG: RULE: :-e_a(Q,O),not supp_e_a(Q,O).");
									{
										Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										}
										{
											OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											// notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
											notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));

											//disctinct between concept and role queries

											notsupp.tuple.push_back(eatom.inputs[0]);
											notsupp.tuple.push_back(eatom.inputs[1]);
											notsupp.tuple.push_back(eatom.inputs[2]);
											notsupp.tuple.push_back(eatom.inputs[3]);
											notsupp.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												notsupp.tuple.push_back(cQID);
												notsupp.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												notsupp.tuple.push_back(rQID);
												notsupp.tuple.push_back(varoID1);
												notsupp.tuple.push_back(varoID2);
											} else {assert(false);}

											rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
										}
										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}

									foundOrdinaryAtom = true;

								}

							}

							if (!foundOrdinaryAtom) {

								DBGLOG(DBG, "RMG: There are no ordinary atoms in support set");
								DBGLOG(DBG, "RMG: RULE: bar_aux_o(C,X):-aux_o(C,X), n_e_a(Q,O). ");

								// bar_aux_o(C,X):-aux_o(C,X), n_e_a(Q,O)
								{
									Rule rule(ID::MAINKIND_RULE);

									// HEAD: bar_aux_o(C,X)
									{
										OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
										headat.tuple.push_back(guardbarPredicateID);
										headat.tuple.push_back(cID);

										if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
											headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
										}
										else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
											headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
										} else assert(false);

										rule.head.push_back(reg->storeOrdinaryAtom(headat));
									}

									// aux_o(C,X)

									{
										OrdinaryAtom boa(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
										boa.tuple.push_back(guardPredicateID);
										//distinct r and c
										boa.tuple.push_back(cID);
										if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
										}
										else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
										} else assert(false);

										rule.body.push_back(reg->storeOrdinaryAtom(boa));
									}

									// n_e_a(Q,O)
									{
										OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}

										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
									}
									ID ruleID = reg->storeRule(rule);

									program.idb.push_back(ruleID);

									DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
								}

								DBGLOG(DBG, "RMG: RULE: supp_e_a(Q,O):-aux_o(C,X),e_a(Q,O), not bar_aux_o(C,X).");
								// * supp_e_a("Q",O):-aux_o("C",X),e_a("Q",O), not bar_aux_o("C",X).
								{
									Rule rule(ID::MAINKIND_RULE);
									{
										// HEAD supp_e_a("Q",O)
										OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										// headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
										headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
										headat.tuple.push_back(eatom.inputs[0]);
										headat.tuple.push_back(eatom.inputs[1]);
										headat.tuple.push_back(eatom.inputs[2]);
										headat.tuple.push_back(eatom.inputs[3]);
										headat.tuple.push_back(eatom.inputs[4]);

										if (cQID!=ID_FAIL) {
											headat.tuple.push_back(cQID);
											headat.tuple.push_back(varoID);
										}
										else if (rQID!=ID_FAIL) {
											headat.tuple.push_back(rQID);
											headat.tuple.push_back(varoID1);
											headat.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.head.push_back(reg->storeOrdinaryAtom(headat));
									}

									//aux_o("C",X)
									{
										OrdinaryAtom boa(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
										boa.tuple.push_back(guardPredicateID);
										boa.tuple.push_back(cID);
										if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
										}
										else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											boa.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
										} else assert(false);

										rule.body.push_back(reg->storeOrdinaryAtom(boa));
									}

									//e_a("Q",O)

									{
										OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
									}

									//not bar_aux_o("C",X)
									{
										OrdinaryAtom notbarat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										notbarat.tuple.push_back(guardbarPredicateID);
										//distinct
										notbarat.tuple.push_back(cID);
										if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==3) {
											notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
										}
										else if (reg->lookupOrdinaryAtom(guardIDOrig).tuple.size()==4) {
											notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
											notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[3]);
										} else assert(false);
										rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notbarat)));
									}
									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);
								}

								DBGLOG(DBG, "RMG: RULE: :-e_a(Q,O),not supp_e_a(Q,O).");
								// * :-e_a("Q",O),not supp_e_a("Q",O).
								{
									Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
									{
										OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
										repl.tuple.push_back(eatom.inputs[0]);
										repl.tuple.push_back(eatom.inputs[1]);
										repl.tuple.push_back(eatom.inputs[2]);
										repl.tuple.push_back(eatom.inputs[3]);
										repl.tuple.push_back(eatom.inputs[4]);
										//distinct

										if (cQID!=ID_FAIL) {
											repl.tuple.push_back(cQID);
											repl.tuple.push_back(varoID);
										}
										else if (rQID!=ID_FAIL) {
											repl.tuple.push_back(rQID);
											repl.tuple.push_back(varoID1);
											repl.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
									}
									{
										OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
										// notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
										notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
										//distinct
										notsupp.tuple.push_back(eatom.inputs[0]);
										notsupp.tuple.push_back(eatom.inputs[1]);
										notsupp.tuple.push_back(eatom.inputs[2]);
										notsupp.tuple.push_back(eatom.inputs[3]);
										notsupp.tuple.push_back(eatom.inputs[4]);

										if (cQID!=ID_FAIL) {
											notsupp.tuple.push_back(cQID);
											notsupp.tuple.push_back(varoID);
										}
										else if (rQID!=ID_FAIL) {
											notsupp.tuple.push_back(rQID);
											notsupp.tuple.push_back(varoID1);
											notsupp.tuple.push_back(varoID2);
										} else {assert(false);}
										rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
									}
									ID ruleID = reg->storeRule(rule);
									DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
									program.idb.push_back(ruleID);
								}

							}
						}
						// no guard just predicate update
						else if (guardID == ID_FAIL) {
							BOOST_FOREACH (ID id, ng) {

								ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));
								const OrdinaryAtom& oatom = reg->lookupOrdinaryAtom(idOrig);
								if (oatom.tuple[0] == eatom.inputs[1]) {

									// it is c+
									// create rules:
									DBGLOG(DBG, "RMG: RULE: :-aux_p(D,Y), n_e_a(Q,O).");

									// * :-aux_p("D",Y), n_e_a("Q",O). (neg. repl. of eatom)
									{
										Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
										rule.body.push_back(idOrig);
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);

											// ditinct c and r
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}

											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										}

										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}

									DBGLOG(DBG,"RMG: RULE: supp_e_a(Q,O):-e_a(Q,O), aux_p(D,Y)");
									// * supp_e_a("Q",O):-e_a("Q",O), aux_p("D",Y).
									{
										Rule rule(ID::MAINKIND_RULE);
										{
											OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											// headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
											headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
											headat.tuple.push_back(eatom.inputs[0]);
											headat.tuple.push_back(eatom.inputs[1]);
											headat.tuple.push_back(eatom.inputs[2]);
											headat.tuple.push_back(eatom.inputs[3]);
											headat.tuple.push_back(eatom.inputs[4]);

											if (cQID!=ID_FAIL) {
												headat.tuple.push_back(cQID);
												headat.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												headat.tuple.push_back(rQID);
												headat.tuple.push_back(varoID1);
												headat.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.head.push_back(reg->storeOrdinaryAtom(headat));
										}
										rule.body.push_back(ID::posLiteralFromAtom(idOrig));
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));

										}

										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}

									DBGLOG(DBG, "RMG: RULE: :-e_a(Q,O),not supp_e_a(Q,O)");
									// * :-e_a("Q",O),not supp_e_a("Q",O).
									{
										Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
										{
											OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
											repl.tuple.push_back(eatom.inputs[0]);
											repl.tuple.push_back(eatom.inputs[1]);
											repl.tuple.push_back(eatom.inputs[2]);
											repl.tuple.push_back(eatom.inputs[3]);
											repl.tuple.push_back(eatom.inputs[4]);
											if (cQID!=ID_FAIL) {
												repl.tuple.push_back(cQID);
												repl.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												repl.tuple.push_back(rQID);
												repl.tuple.push_back(varoID1);
												repl.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
										}
										{
											OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
											// notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
											notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.innerEatoms[eaIndex]));
											// distinct
											notsupp.tuple.push_back(eatom.inputs[0]);
											notsupp.tuple.push_back(eatom.inputs[1]);
											notsupp.tuple.push_back(eatom.inputs[2]);
											notsupp.tuple.push_back(eatom.inputs[3]);
											notsupp.tuple.push_back(eatom.inputs[4]);

											if (cQID!=ID_FAIL) {
												notsupp.tuple.push_back(cQID);
												notsupp.tuple.push_back(varoID);
											}
											else if (rQID!=ID_FAIL) {
												notsupp.tuple.push_back(rQID);
												notsupp.tuple.push_back(varoID1);
												notsupp.tuple.push_back(varoID2);
											} else {assert(false);}
											rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
										}
										ID ruleID = reg->storeRule(rule);
										DBGLOG(DBG, "RMG: RULE: Adding rule: " << RawPrinter::toString(reg, ruleID));
										program.idb.push_back(ruleID);
									}
								}
							}
						}
					}
				}

				DBGLOG(DBG, "RMG: Adding Abox");
				InterpretationPtr edb(new Interpretation(reg));
				edb->add(*program.edb);
				DBGLOG(DBG, " Program edb before adding ABox "<<*edb);
				program.edb = edb;
				DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(factory.ctx, reg->storeConstantTerm(factory.ctx.getPluginData<DLLitePlugin>().repairOntology));

				// add ontology ABox in form of facts aux_o("D",c)
				edb->add(*ontology->conceptAssertions);

				// (accordingly aux_o("R",c1,c2)).
				BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion ra, ontology->roleAssertions) {
					OrdinaryAtom roleAssertion(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
					roleAssertion.tuple.push_back(guardPredicateID);
					roleAssertion.tuple.push_back(ra.first);
					roleAssertion.tuple.push_back(ra.second.first);
					roleAssertion.tuple.push_back(ra.second.second);
					edb->setFact(reg->storeOrdinaryAtom(roleAssertion).address);
				}
				DBGLOG(DBG, "RMG: Program edb after adding ABox "<<*edb);
				// DBGLOG(DBG, "RMG: program edb: ");

				// add atoms for the case when support sets are complete


				bm::bvector<>::enumerator enm;
				bm::bvector<>::enumerator enm_end;
				enm = edb->getStorage().first();
				enm_end = edb->getStorage().end();

				/*while (enm < enm_end){
				 ID id = reg->ogatoms.getIDByAddress(*enm);
				 DBGLOG(DBG,"RMG: "<<RawPrinter::toString(reg,id)<<".");
				 enm++;
				 }*/

				DBGLOG(DBG, "RMG: program idb: ");
				for (unsigned ruleIndex=0; ruleIndex<program.idb.size(); ruleIndex++) {
					DBGLOG(DBG, "RMG: "<<RawPrinter::toString(reg,program.idb[ruleIndex]));
				}

				// ground the program and evaluate it
				// get the results, filter them out with respect to only relevant predicates (all apart from aux_o, replacement atoms)
				grounder = GenuineGrounder::getInstance(factory.ctx, program);
				DBGLOG(DBG, "RMG: after grounding");
				// annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.allEatoms);
				annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.innerEatoms);
				DBGLOG(DBG, "RMG: annotated ground program is constructed");
				solver = GenuineGroundSolver::getInstance(
						factory.ctx, annotatedGroundProgram,
						// no interleaved threading because guess and check MG will likely not profit from it
						InterpretationConstPtr(),
						// do the UFS check for disjunctions only if we don't do
						// a minimality check in this class;
						// this will not find unfounded sets due to external sources,
						// but at least unfounded sets due to disjunctions
						!factory.ctx.config.getOption("FLPCheck") && !factory.ctx.config.getOption("UFSCheck"));
				DBGLOG(DBG, "RMG: after calling the solver");
				nogoodGrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), learnedEANogoods, learnedEANogoods, annotatedGroundProgram));
				DBGLOG(DBG, "RMG: after creating a nogood grounder");

#if 0 // ground the support sets exhaustively
				DBGLOG(DBG, "RMG: start grounding supports sets");
				NogoodGrounderPtr nogoodgrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), potentialSupportSets, potentialSupportSets, annotatedGroundProgram));

				int nc = 0;
				while (nc < potentialSupportSets->getNogoodCount()) {
					nc = potentialSupportSets->getNogoodCount();
					DBGLOG(DBG, "RMG: number of support sets is "<<nc);
					nogoodgrounder->update();
				}
				DLVHEX_BENCHMARK_REGISTER(sidgroundpsupportsets, "ground potential supportsets");
				DLVHEX_BENCHMARK_COUNT(sidgroundpsupportsets, supportSets->getNogoodCount());

				// some support sets are also learned nogoods
				DBGLOG(DBG, "RMG: after grounding number of support sets is "<<potentialSupportSets->getNogoodCount());

				DBGLOG(DBG,"RMG: decide which support sets to keep");
				bool keep;
				bool isGuard;
				for (int i = 0; i < potentialSupportSets->getNogoodCount(); ++i) {
					const Nogood& ng = potentialSupportSets->getNogood(i);
					DBGLOG(DBG, "RMG: current support set "<<i<<" namely " << ng.getStringRepresentation(reg));
					DBGLOG(DBG, "RMG: is it ground?");
					if (ng.isGround()) {
						DBGLOG(DBG, "RMG: yes");
						// determine whether it has a guard
						DBGLOG(DBG, "RMG: does it have guard atom?");
						isGuard=false;
						BOOST_FOREACH (ID lit, ng) {
							ID litID = reg->ogatoms.getIDByAddress(lit.address);
							if (!isGuard)
							if (litID.isAuxiliary()) {
								const OrdinaryAtom& possibleGuardAtom = reg->lookupOrdinaryAtom(lit);
								if (possibleGuardAtom.tuple[0]==theDLLitePlugin.guardPredicateID) {
									isGuard=true;
								}
							}
							if (isGuard) {
								DBGLOG(DBG, "RMG: yes, support set "<<ng.getStringRepresentation(reg)<<" has a guard, do not add it to nogoods");
							}
							else DBGLOG(DBG, "RMG: no");

						}

						// determine the external atom replacement in ng
						ID eaAux = ID_FAIL;

						BOOST_FOREACH (ID lit, ng) {
							if (reg->ogatoms.getIDByAddress(lit.address).isExternalAuxiliary()) {
								if (eaAux != ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains multiple external literals");
								eaAux = lit;
							}
						}
						if (eaAux == ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains no external literals");

						if (annotatedGroundProgram.mapsAux(eaAux.address)) {
							DBGLOG(DBG, "RMG: evaluating guards (if there are any) of " << ng.getStringRepresentation(reg));
							keep = true;
							Nogood ng2 = ng;
							reg->eatoms.getByID(annotatedGroundProgram.getAuxToEA(eaAux.address)[0]).pluginAtom->guardSupportSet(keep, ng2, eaAux);
							if (keep) {
#ifdef DEBUG
								// ng2 must be a subset of ng and still a valid support set
								ID aux = ID_FAIL;
								BOOST_FOREACH (ID id, ng2) {
									if (reg->ogatoms.getIDByAddress(id.address).isExternalAuxiliary()) aux = id;
									assert(std::find(ng.begin(), ng.end(), id) != ng.end());
								}
								assert(aux != ID_FAIL);
#endif
								DBGLOG(DBG, "RMG: keeping " << ng.getStringRepresentation(reg));
								if (isGuard==false) {
									DBGLOG(DBG, "RMG: support set "<< ng.getStringRepresentation(reg)<<" has no guards");
									DBGLOG(DBG, "RMG: add it to set of nogoods");
									learnedEANogoods->addNogood(ng);
								}
								DBGLOG(DBG, "RMG: add to supportSets");
								supportSets->addNogood(ng);
							}
							else {
								DBGLOG(DBG, "RMG: reject " << ng.getStringRepresentation(reg));
							}
						}
					}
					else DBGLOG(DBG, "RMG: no");
				}
				DBGLOG(DBG, "RMG: finished analyzing support sets");
				DLVHEX_BENCHMARK_REGISTER(sidgroundsupportsets, "final ground supportsets");
				DLVHEX_BENCHMARK_COUNT(sidgroundsupportsets, supportSets->getNogoodCount());

				DBGLOG(DBG, "RMG: add " << supportSets->getNogoodCount() << " support sets to factory");
				factory.supportSets = supportSets;
				DBGLOG(DBG, "RMG: add "<< learnedEANogoods->getNogoodCount()<<" nogoods to annotated program");
#endif	 annotatedGroundProgram.setCompleteSupportSetsForVerification(learnedEANogoods);
			}
		}
	}

	void RepairModelGenerator::updateEANogoods(
			InterpretationConstPtr compatibleSet,
			InterpretationConstPtr factWasSet,
			InterpretationConstPtr changed) {

		// generalize ground nogoods to nonground ones
		if (factory.ctx.config.getOption("ExternalLeaok, now its clearrningGeneralize")) {
			int max = learnedEANogoods->getNogoodCount();
			for (int i = learnedEANogoodsTransferredIndex; i < max; ++i) {
				generalizeNogood(learnedEANogoods->getNogood(i));
			}
		}

		// instantiate nonground nogoods
		if (factory.ctx.config.getOption("NongroundNogoodInstantiation")) {
			nogoodGrounder->update(compatibleSet, factWasSet, changed);
		}

		// transfer nogoods to the solver
		DLVHEX_BENCHMARK_REGISTER_AND_COUNT(sidcompatiblesets, "Learned EA-Nogoods", learnedEANogoods->getNogoodCount() - learnedEANogoodsTransferredIndex);
		for (int i = learnedEANogoodsTransferredIndex; i < learnedEANogoods->getNogoodCount(); ++i) {
			const Nogood& ng = learnedEANogoods->getNogood(i);
			if (factory.ctx.config.getOption("PrintLearnedNogoods")) {
				// we cannot use i==1 because of learnedEANogoods.clear() below in this function
				static bool first = true;
				if( first )
				{
					if (factory.ctx.config.getOption("GenuineSolver") >= 3) {
						LOG(DBG, "( NOTE: With clasp backend, learned nogoods become effective with a delay because of multithreading! )");
					} else {
						LOG(DBG, "( NOTE: With i-backend, learned nogoods become effective AFTER the next model was printed ! )");
					}
					first = false;
				}
				LOG(DBG,"learned nogood " << ng.getStringRepresentation(reg));
			}
			if (ng.isGround()) {
				solver->addNogood(ng);
			} else {
				// keep nonground nogoods beyond the lifespan of this model generator
				factory.globalLearnedEANogoods->addNogood(ng);
			}
		}

		// for encoding-based UFS checkers and explicit FLP checks, we need to keep learned nogoods (otherwise future UFS searches will not be able to use them)
		// for assumption-based UFS checkers we can delete them as soon as nogoods were added both to the main search and to the UFS search
		if (factory.ctx.config.getOption("UFSCheckAssumptionBased") ||
				(annotatedGroundProgram.hasECycles() == 0 && factory.ctx.config.getOption("FLPDecisionCriterionE"))) {
			ufscm->learnNogoodsFromMainSearch(true);
			//	nogoodGrounder->resetWatched(learnedEANogoods);
			learnedEANogoods->clear();
		} else {
			learnedEANogoods->forgetLeastFrequentlyAdded();
		}
		learnedEANogoodsTransferredIndex = learnedEANogoods->getNogoodCount();
	}

	bool RepairModelGenerator::postCheck(InterpretationConstPtr modelCandidate) {
		DBGLOG(DBG,"EL: RMG: PC: evaluation post check is started:");

		DBGLOG(DBG,"EL: RMG: PC: current model candidate is: "<< *modelCandidate);

		// boolean variable which stores the evaluation result
		bool evalsucc=true;

		// extract repair ABox candidate from the model


		// vector for storing IDs of original ABox assertions
		std::vector<ID> ab;

		//vector for storing assertions that have to be removed (extracted from bar guards)
		std::vector<ID> removal;

		//vector for storing new ABox
		std::vector<ID> newab;

		ID guardPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 0));
		ID guardbarPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 1));



		bm::bvector<>::enumerator nea = modelCandidate->getStorage().first();
		bm::bvector<>::enumerator nea_end = modelCandidate->getStorage().end();


		while (nea < nea_end) {
			ID id = reg->ogatoms.getIDByAddress(*nea);
			if (reg->ogatoms.getByID(id).tuple[0]==guardbarPredicateID) {
				removal.push_back(id);
				//DBGLOG(DBG,"EL: RMG: PC: remove "<< RawPrinter::toString(reg,id));
			}

			else if (reg->ogatoms.getByID(id).tuple[0]==guardPredicateID) {
				ab.push_back(id);
				//DBGLOG(DBG,"EL: RMG: abox assertion "<< RawPrinter::toString(reg,id));
			}
			nea++;
		}

		// go through facts in ab and remove all those that have the same parameters in removal

		std::vector<ID>::iterator it = ab.begin();
		std::vector<ID>::iterator it_end = ab.end();


		while (it<it_end) {
			ID idab = ID(*it);
			OrdinaryAtom aboxfact = reg->ogatoms.getByID(idab);

			//DBGLOG(DBG,"EL: RMG: abox fact is "<<RawPrinter::toString(reg,idab));

			OrdinaryAtom a(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
			a.tuple.push_back(guardbarPredicateID);


			for (int i=1; i<aboxfact.tuple.size();i++) {
				a.tuple.push_back(reg->ogatoms.getByID(idab).tuple[i]);
			}

			ID delID = reg->storeOrdinaryGAtom(a);

			if ((std::find(removal.begin(), removal.end(), delID) != removal.end())) {
				//DBGLOG(DBG,"EL: RMG: PC: do not add "<<RawPrinter::toString(reg,idab)<<" to new ABox");
			}
			else {
				//DBGLOG(DBG,"EL: RMG: PC: add "<<RawPrinter::toString(reg,idab)<<" to new ABox");
				newab.push_back(idab);
			}
			it++;
		}



		DBGLOG(DBG,"EL: RMG: final ABox is:");

		std::vector<ID>::iterator newa = newab.begin();
		std::vector<ID>::iterator newa_end = newab.end();

		while (newa<newa_end) {
			ID idab = ID(*newa);
			DBGLOG(DBG,"EL: RMG: "<<RawPrinter::toString(reg,idab));
			newa++;
		}



		bm::bvector<>::enumerator enm = modelCandidate->getStorage().first();
		bm::bvector<>::enumerator enm_end = modelCandidate->getStorage().end();

		DBGLOG(DBG,"EL: RMG: PC: going through facts of the current model candidate: ");

		ID evid = reg->getAuxiliaryConstantSymbol('e', ID(0,0));
		OrdinaryAtom ev(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
		ev.tuple.push_back(evid);


		DBGLOG(DBG,"EL: RMG: PC: if " << RawPrinter::toString(reg,evid)<< " is present in the model then the corresponding external atom needs to be evaluated");

		std::vector<ID> evalatoms;
		while (enm < enm_end) {
			ID id = reg->ogatoms.getIDByAddress(*enm);
			DBGLOG(DBG,"EL: RMG: PC: current atoms is: "<< RawPrinter::toString(reg,id)<<" with tuple "<<RawPrinter::toString(reg,reg->ogatoms.getByID(id).tuple[0]));
			//DBGLOG(DBG,"EL: RMG: PC: compare "<<RawPrinter::toString(reg,reg->ogatoms.getByID(id).tuple[0])<<" with "<< RawPrinter::toString(reg,evid));
			//DBGLOG(DBG,"EL: RMG: PC: IDs are: "<<reg->ogatoms.getByID(id).tuple[0]<<" and "<< evid);

			if (reg->ogatoms.getByID(id).tuple[0]==evid) {
				DBGLOG(DBG,"EL: RMG: this is eval atom, search for its twin among external ones");

				DBGLOG(DBG,"EL: RMG: number of external atoms: "<<factory.innerEatoms.size());
				for (unsigned eaIndex=0; eaIndex<factory.innerEatoms.size();eaIndex++) {
					const ExternalAtom& eatom = reg->eatoms.getByID(factory.innerEatoms[eaIndex]);

					ID eatID = factory.innerEatoms[eaIndex];
					DBGLOG(DBG,"EL: RMG: consider atom "<< RawPrinter::toString(reg,eatID));
					bool ev=true;
					for (int i=1; i<7;i++) {
						ID idev = reg->ogatoms.getByID(id).tuple[i];
						ID idea = reg->eatoms.getByID(eatID).inputs[i-1];
						if (idev!=idea) {
							DBGLOG(DBG,"EL: RMG: eval.tuple["<<i<<"] = "<<RawPrinter::toString(reg, reg->ogatoms.getByID(id).tuple[i]));
							DBGLOG(DBG,"EL: RMG: extatom.input["<<i-1<<"] = "<<RawPrinter::toString(reg, reg->eatoms.getByID(factory.innerEatoms[eaIndex]).inputs[i-1]));
							ev=false;
						}

					}

					ID cQID = ID_FAIL;
					ID rQID = ID_FAIL;

					ID cdlID = reg->storeConstantTerm("cDL");
					ID rdlID = reg->storeConstantTerm("rDL");

					if (eatom.predicate==cdlID) {
						// query is a concept
						cQID = eatom.inputs[5];
					}

					else if (eatom.predicate==rdlID) {
						//query is a role
						rQID = eatom.inputs[5];
					}

					else assert(false);

					if (ev)
					{
						// the atom with index eaIndex has to be evaluated
						// to fix the relevant input we create new auxiliary ontology predicates (for concepts and roles)
						ID newauxIDun = theDLLitePlugin.storeQuotedConstantTerm("000");
						//ID newauxIDbin = theDLLitePlugin.storeQuotedConstantTerm("111");

						if (cQID!=ID_FAIL) {
							DBGLOG(DBG,"EL: RMG: PC: evaluate "<< RawPrinter::toString(reg,eatID)<<" for a constant "<<RawPrinter::toString(reg,reg->ogatoms.getByID(id).tuple[7]));

							// create a copy of interpretation for input
							Interpretation::Ptr postcheckInput;
							postcheckInput.reset(new Interpretation(*modelCandidate));
							DBGLOG(DBG,"EL: RMG: PC: copy of interpretation for input is created");

							//create a copy of interpretation for output
							Interpretation::Ptr postcheckOutput;
							postcheckOutput.reset(new Interpretation(reg));
							DBGLOG(DBG,"EL: RMG: PC: interpretation for output is created");

							// first create an auxiliary concept which will be used for specifying which constants are of interest for us
							OrdinaryAtom relev(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							relev.tuple.push_back(eatom.inputs[1]);
							relev.tuple.push_back(newauxIDun);
							relev.tuple.push_back(reg->ogatoms.getByID(id).tuple[7]);
							ID relevID = reg->storeOrdinaryGAtom(relev);

							DBGLOG(DBG,"EL: RMG: PC: stored atom is "<<RawPrinter::toString(reg,relevID));

							postcheckInput->setFact(relevID.address);

							// second specify that the new ABox needs to be used and add this ABox as facts w.r.t input atoms
							OrdinaryAtom usenewab(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							usenewab.tuple.push_back(eatom.inputs[1]);
							ID usenewabID = reg->storeOrdinaryAtom(usenewab);

							DBGLOG(DBG,"EL: RMG: PC stored atom is "<<RawPrinter::toString(reg,usenewabID));


							postcheckInput->setFact(usenewabID.address);

							// add abox assertions to the input predicates


							DBGLOG(DBG,"EL: RMG: go through new ABox");

							newa = newab.begin();
							newa_end = newab.end();


						while (newa<newa_end) {
								ID idadd = ID(*newa);


								DBGLOG(DBG,"EL: RMG: current ABox fact: "<<RawPrinter::toString(reg,idadd));

								OrdinaryAtom addat=reg->ogatoms.getByID(idadd);

								int size = addat.tuple.size();
								DBGLOG(DBG,"EL: size ofthe atom is "<<size);



								OrdinaryAtom abfact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);

								if (size==3) {
									//create a concept
									DBGLOG(DBG,"EL: RMG: tuple size is 3, concept assertion");
									abfact.tuple.push_back(eatom.inputs[1]);
									abfact.tuple.push_back(addat.tuple[1]);
									abfact.tuple.push_back(addat.tuple[2]);

									ID abfactID = reg->storeOrdinaryAtom(abfact);
									DBGLOG(DBG,"EL: RMG: created atom is: "<<RawPrinter::toString(reg,abfactID));

									postcheckInput->setFact(abfactID.address);

								}
								else if (size==4){
									//create a role
									DBGLOG(DBG,"EL: RMG: tuple size is 4, role assertion");

									abfact.tuple.push_back(eatom.inputs[3]);
									abfact.tuple.push_back(addat.tuple[1]);
									abfact.tuple.push_back(addat.tuple[2]);
									abfact.tuple.push_back(addat.tuple[3]);

									ID abfactID = reg->storeOrdinaryAtom(abfact);
									DBGLOG(DBG,"EL: RMG: created atom is: "<<RawPrinter::toString(reg,abfactID));
									postcheckInput->setFact(abfactID.address);
									DBGLOG(DBG,"EL: RMG: added fact to interpretation");


								}
								else assert(false&&"ABox assertion is neither unary nor binary");

								newa++;
							}

							//	DBGLOG(DBG,"EL: RMG: PC: relev atom is "<< RawPrinter::toString(reg,ID::posLiteralFromAtom(reg->storeOrdinaryAtom(relev))));
							//									postcheckInput->setFact(reg->storeOrdinaryAtom(relev).address);

							DBGLOG(DBG,"EL: RMG: PC: after creation of relev atom");

							// evaluate current atom under the extended interpretation and anlyze
							// the output values w.r.t. to the current model candidate
							DBGLOG(DBG,"EL: RMG: Before evaluation");

							IntegrateExternalAnswerIntoInterpretationCB cb(postcheckOutput);
							std::vector<ID> evat;
							evat.push_back(factory.innerEatoms[eaIndex]);
							evaluateExternalAtoms(factory.ctx, evat, postcheckInput, cb);

							DBGLOG(DBG,"EL: RMG: After evaluation");
							evalsucc=false;
							bm::bvector<>::enumerator e = postcheckOutput->getStorage().first();
							bm::bvector<>::enumerator e_end = postcheckOutput->getStorage().end();

							while (e < e_end) {
								ID i = reg->ogatoms.getIDByAddress(*e);
								DBGLOG(DBG,"EL: RMG: PC: current element of postcheck is: "<< RawPrinter::toString(reg,i));
								bm::bvector<>::enumerator a = modelCandidate->getStorage().first();
								bm::bvector<>::enumerator a_end = modelCandidate->getStorage().end();
								DBGLOG(DBG,"EL: RMG: PC: go through elements of the model and check whether the id of the evaluated atom occurs in it");

								while (a < a_end) {
									ID i1 = reg->ogatoms.getIDByAddress(*a);
									DBGLOG(DBG,"EL: RMG: PC: atom in the model: "<< RawPrinter::toString(reg, i1));
									if (i==i1) {
										evalsucc = true;
										DBGLOG(DBG,"EL: RMG: PC: target atom "<< RawPrinter::toString(reg, i)<<" is found");
									}
									else {
										DBGLOG(DBG,"EL: RMG: PC: search further");
									}
									a++;
								}

								e++;
							}
							if (evalsucc==false) {
								DBGLOG(DBG,"EL: RMG: PC: evaluation showed that the model is not a repair answer set");
								return false;}

							else {
								DBGLOG(DBG,"EL: RMG: PC: so far evaluation results coincide with the ones encoded in the model candidate");
							}

						}

						else if (rQID!=ID_FAIL) {
							DBGLOG(DBG,"EL: RMG: PC: current atom is a role");
							DBGLOG(DBG,"EL: RMG: PC: it needs to be evaluated for a pair of constants  ("<<reg->ogatoms.getByID(id).tuple[7]<<","<<reg->ogatoms.getByID(id).tuple[8]<<")");

							// check if query grounded by the relevant constant is in the ontology

							OrdinaryAtom abfact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							abfact.tuple.push_back(guardPredicateID);
							abfact.tuple.push_back(rQID);
							abfact.tuple.push_back(reg->ogatoms.getByID(id).tuple[7]);
							abfact.tuple.push_back(reg->ogatoms.getByID(id).tuple[8]);

							OrdinaryAtom inputfact(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG);
							inputfact.tuple.push_back(eatom.inputs[1]);
							inputfact.tuple.push_back(rQID);
							inputfact.tuple.push_back(reg->ogatoms.getByID(id).tuple[7]);
							inputfact.tuple.push_back(reg->ogatoms.getByID(id).tuple[8]);

							if ((std::find(newab.begin(), newab.end(), reg->ogatoms.getIDByStorage(abfact)) != newab.end())||(modelCandidate->getFact(reg->ogatoms.getIDByStorage(inputfact))))
							{

								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
								repl.tuple.push_back(eatom.inputs[0]);
								repl.tuple.push_back(eatom.inputs[1]);
								repl.tuple.push_back(eatom.inputs[2]);
								repl.tuple.push_back(eatom.inputs[3]);
								repl.tuple.push_back(eatom.inputs[4]);
								repl.tuple.push_back(rQID);
								repl.tuple.push_back(reg->ogatoms.getByID(id).tuple[7]);
								repl.tuple.push_back(reg->ogatoms.getByID(id).tuple[8]);

								if (!modelCandidate->getFact(reg->ogatoms.getIDByStorage(repl))) {
									DBGLOG(DBG,"EL: RMG: PC: evaluation of a current atom dod not succed");
									return false;
								}
							}

							else {

								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
								repl.tuple.push_back(eatom.inputs[0]);
								repl.tuple.push_back(eatom.inputs[1]);
								repl.tuple.push_back(eatom.inputs[2]);
								repl.tuple.push_back(eatom.inputs[3]);
								repl.tuple.push_back(eatom.inputs[4]);
								repl.tuple.push_back(rQID);
								repl.tuple.push_back(reg->ogatoms.getByID(id).tuple[7]);
								repl.tuple.push_back(reg->ogatoms.getByID(id).tuple[8]);

								if (!modelCandidate->getFact(reg->ogatoms.getIDByStorage(repl))) {
									DBGLOG(DBG,"EL: RMG: PC: evaluation of a current atom did not succed");
									return false;
								}


						}

					}

					else assert(false&&"External atom is neither unary nor binary");

					eaIndex=factory.innerEatoms.size();
				}

			}
		}
		enm++;
	}
	DBGLOG(DBG,"EL: RMG: PC: postcheck is finished");

	return true;

}

bool RepairModelGenerator::repairCheck(InterpretationConstPtr modelCandidate) {

	DBGLOG(DBG,"RMG: repair check is started:");
	DBGLOG(DBG,"RMG: (Result) current model candidate is: "<< *modelCandidate);

	// repair exists is a flag that witnesses the ABox repair existence
	bool repairexists;
	bool emptyrepair;
	repairexists = true;
	int ngCount;

	//	DBGLOG(DBG,"RMG: number of all external atoms: " << factory.allEatoms.size());
	DBGLOG(DBG,"RMG: number of all external atoms: " << factory.innerEatoms.size());

	// We divide all external atoms into two groups:
	// group dpos: those that were guessed true in modelCandidate;
	// group npos: and those that were guessed false in modelCandidate.

	InterpretationPtr dpos(new Interpretation(reg));
	InterpretationPtr dneg(new Interpretation(reg));

	// Go through all atoms in alleatoms
	// and evaluate them
	// mask stores all relevant atoms for external atom with index eaindex
	//for (unsigned eaIndex=0; eaIndex<factory.allEatoms.size();eaIndex++){
	for (unsigned eaIndex=0; eaIndex<factory.innerEatoms.size();eaIndex++) {
		//		DBGLOG(DBG,"RMG: consider atom "<< eaIndex<<", namely "<< RawPrinter::toString(reg,factory.allEatoms[eaIndex]));
		DBGLOG(DBG,"RMG: consider atom "<< eaIndex<<", namely "<< RawPrinter::toString(reg,factory.innerEatoms[eaIndex]));
		annotatedGroundProgram.getEAMask(eaIndex)->updateMask();

		const InterpretationConstPtr& mask = annotatedGroundProgram.getEAMask(eaIndex)->mask();

		DBGLOG(DBG,"RMG: mask is created "<< *mask);

		bm::bvector<>::enumerator enm;
		bm::bvector<>::enumerator enm_end;
		// make sure that ALL input auxiliary atoms are true, otherwise we might miss some output atoms and consider true output atoms wrongly as unfounded
		// thus we need the following:
		InterpretationPtr evalIntr(new Interpretation(reg));

		if (!factory.ctx.config.getOption("IncludeAuxInputInAuxiliaries")) {
			// clone and extend
			evalIntr->getStorage() |= annotatedGroundProgram.getEAMask(eaIndex)->getAuxInputMask()->getStorage();
		}

		// analyze the current external atom

		enm = mask->getStorage().first();
		enm_end = mask->getStorage().end();

		DBGLOG(DBG,"RMG: go through elements of mask ");
		while (enm < enm_end) {
			ID id = reg->ogatoms.getIDByAddress(*enm);
			DBGLOG(DBG,"RMG: atom "<<RawPrinter::toString(reg,id));

			if (id.isExternalAuxiliary() && !id.isExternalInputAuxiliary()) {
				// it is an external atom replacement, now check if it is positive or negative
				DBGLOG(DBG,"RMG: replacement");
				if (reg->isPositiveExternalAtomAuxiliaryAtom(id)) {
					DBGLOG(DBG,"RMG: positive");
					// add it to dpos
					if (modelCandidate->getFact(*enm)) {
						DBGLOG(DBG,"RMG: guessed true");
						DBGLOG(DBG,"RMG: add " << RawPrinter::toString(reg,id) << " to dpos ");
						dpos->setFact(*enm);
					}
					else DBGLOG(DBG,"RMG: guessed false");

				}
				else {DBGLOG(DBG,"RMG: negative"); // it is negative
					if (modelCandidate->getFact(*enm)) {
						DBGLOG(DBG,"RMG: guessed true");
						ID inverse = reg->swapExternalAtomAuxiliaryAtom(id);
						DBGLOG(DBG,"RMG: add atom " << RawPrinter::toString(reg,inverse) << " to set dneg ");
						dneg->setFact(reg->swapExternalAtomAuxiliaryAtom(id).address);
					}
					else DBGLOG(DBG,"RMG: guessed false");
					// assertion is not needed, but should be added to make the implementation more rebust
					// (it might help to find programming errors later on)
				}
				assert(reg->isNegativeExternalAtomAuxiliaryAtom(id) && "replacement atom is neither positive nor negative");
			}
			else //  it is not a replacement atom
			DBGLOG(DBG,"nonreplacement");
			enm++;
		}
		DBGLOG(DBG,"RMG: finished with atom "<< eaIndex);
	}
	DBGLOG(DBG,"RMG: got out of the loop that sorts replacement atoms to dpos and dneg ");

	// Set create a temporary repairABox and store there original ABox
	DBGLOG(DBG,"RMG: create temporary ABox");
	DLLitePlugin::CachedOntologyPtr newOntology = theDLLitePlugin.prepareOntology(factory.ctx, reg->storeConstantTerm(factory.ctx.getPluginData<DLLitePlugin>().repairOntology));
	InterpretationPtr newConceptsABoxPtr = newOntology->conceptAssertions;
	InterpretationPtr newConceptsABox(new Interpretation(reg));
	newConceptsABox->add(*newConceptsABoxPtr);
	std::vector<DLLitePlugin::CachedOntology::RoleAssertion> newRolesABox = newOntology->roleAssertions;

	DLLitePlugin::CachedOntologyPtr delontology;

	/*DLLitePlugin::CachedOntologyPtr delOntology;
	 InterpretationPtr delConceptsABoxPtr = delOntology->conceptAssertions;
	 InterpretationPtr delConceptsABox(new Interpretation(reg));
	 delConceptsABox->add(*delConceptsABoxPtr);
	 std::vector<DLLitePlugin::CachedOntology::RoleAssertion> delRolesABox = delOntology->roleAssertions;
	 */
	DBGLOG(DBG,"RMG: create map (id of atom)->set of supp sets for it ");
	// create a map that maps id of external atoms to vector of its support sets
	std::map<ID,std::vector<Nogood> > dlatsupportsets;
	DBGLOG(DBG,"RMG: map is created ");

	// set of IDs of DL-atoms which have support sets which do not contain any guards
	std::vector<ID> dlatnoguard;
	DBGLOG(DBG,"RMG: vector that store atoms without guards is created");

	// go through all stored nogoods
	DBGLOG(DBG,"RMG: go through all " << factory.supportSets->getNogoodCount()<< " support sets");

	for (int i = 0; i<factory.supportSets->getNogoodCount();i++) {
		DBGLOG(DBG,"RMG: consider support set number "<< i <<": " << factory.supportSets->getNogood(i).getStringRepresentation(reg) << " is considered");

		// keep is a flag that identifies whether a certain support set for a DL-atom should be kept and added to the map
		bool keep = true;
		bool hasAuxiliary = false;
		ID currentExternalId;

		DBGLOG(DBG,"RMG: go through its literals");
		BOOST_FOREACH(ID id,factory.supportSets->getNogood(i)) {
			// distinct between ordinary atoms, replacement atoms and the guards

			ID newid = reg->ogatoms.getIDByAddress(id.address);
			DBGLOG(DBG,"RMG: literal: "<< RawPrinter::toString(reg,id));

			// if the atom is replacement atom, then store its id in currentExternalId
			if (newid.isExternalAuxiliary()) {
				DBGLOG(DBG,"RMG: replacement atom");

				if (reg->isPositiveExternalAtomAuxiliaryAtom(newid)) {
					DBGLOG(DBG,"RMG: positive");
					currentExternalId = newid;
				}
				else {
					DBGLOG(DBG,"RMG: negative");
					currentExternalId = reg->swapExternalAtomAuxiliaryAtom(newid);
				}

				// if the current replacement atom is not already present in the map then add it to the map
				if (dlatsupportsets.count(currentExternalId)==0) {
					DBGLOG(DBG,"RMG: not yet in map");

					std::vector<Nogood> supset;
					dlatsupportsets[currentExternalId] = supset;
				}
				else DBGLOG(DBG,"RMG: it is in map already");
			}
			// if the current atom is a guard then do nothing
			else if (newid.isGuardAuxiliary()) { //guard atom
				DBGLOG(DBG,"RMG: guard atom");
				// the current support set for the current atom has a guard
				hasAuxiliary = true;
			}

			// if the current atom is an ordinary atom then check whether it is true in the current model and if it is then
			else { // ordinary input atom
				DBGLOG(DBG,"RMG: ordinary atom");
				if (modelCandidate->getFact(newid.address)==newid.isNaf()) {
					DBGLOG(DBG,"RMG: false");
					keep = false;
				}
				else DBGLOG(DBG,"RMG: true");
			}
		}
		DBGLOG(DBG,"RMG: finished going through literals");

		if (keep) {
			DBGLOG(DBG,"RMG: add to map: " <<factory.supportSets->getNogood(i).getStringRepresentation(reg)<<" for "<< RawPrinter::toString(reg,currentExternalId));
			dlatsupportsets[currentExternalId].push_back(factory.supportSets->getNogood(i));
			if (hasAuxiliary == false)
			{
				DBGLOG(DBG,"RMG: support set with no auxiliary atoms, we add it to dlatnoguard");
				dlatnoguard.push_back(currentExternalId);
			}
			else DBGLOG(DBG,"RMG: support set has auxiliary atoms");
		}
	}

	bm::bvector<>::enumerator enpos = dpos->getStorage().first();
	bm::bvector<>::enumerator enpos_end = dpos->getStorage().end();
	bm::bvector<>::enumerator enneg = dneg->getStorage().first();
	bm::bvector<>::enumerator enneg_end = dneg->getStorage().end();
	DBGLOG(DBG,"RMG: dpos is "<<*dpos);
	DBGLOG(DBG,"RMG: dneg is "<<*dneg);

	DBGLOG(DBG,"RMG: go through elements in dpos "<<*dpos);

	if (enpos>=enpos_end) {
		// set of positive atoms is empty
		DBGLOG(DBG,"RMG: dpos is empty");
		if (enneg>=enneg_end) {
			DBGLOG(DBG,"RMG: dneg is empty");
			DBGLOG(DBG,"RMG: repair exists, any ABox is a repair");
		}
		else {
			DBGLOG(DBG,"RMG: dneg is nonempty");
			while (enneg < enneg_end) {
				ID idneg=reg->ogatoms.getIDByAddress(*enneg);
				if(std::find(dlatnoguard.begin(), dlatnoguard.end(), idneg) != dlatnoguard.end()) {
					DBGLOG(DBG,"RMG: no repair exists: atom in dneg has support sets with no guards");
					repairexists=false;
					break;
				}
				else {
					DBGLOG(DBG,"RMG: repair exists, empty ABox is repair");
					emptyrepair=true;
					repairexists=true;
					break;
				}
			}
		}
	}
	else {
		DBGLOG(DBG,"RMG: dpos is nonempty");
		if (enneg>=enneg_end) {
			// set of positive atoms is nonempty but set of negative ones is empty
			DBGLOG(DBG,"RMG: dneg is empty");
			DBGLOG(DBG,"RMG: check whether every atom in dpos has at least one support set");
			while (enpos < enpos_end) {
				ID idpos = reg->ogatoms.getIDByAddress(*enpos);
				DBGLOG(DBG,"RMG: consider "<<RawPrinter::toString(reg,idpos));
				if (dlatsupportsets[idpos].empty()) {
					DBGLOG(DBG,"RMG: no repair exists, atom in dpos has no support sets");
					repairexists=false;
					break;
				}
				enpos++;
			}
		}
		else {
			// neither set of positive nor set of negative atoms is empty
			DBGLOG(DBG,"RMG: dneg is nonempty");
			DBGLOG(DBG,"RMG: go through atoms in dpos "<<*dpos);
			emptyrepair=true;
			while (enpos < enpos_end) {
				ID idpos = reg->ogatoms.getIDByAddress(*enpos);
				DBGLOG(DBG,"RMG: consider "<<RawPrinter::toString(reg,idpos));
				DBGLOG(DBG,"RMG: it has "<< dlatsupportsets[idpos].size()<< " support sets");
				if (std::find(dlatnoguard.begin(), dlatnoguard.end(), idpos) != dlatnoguard.end()) {
					DBGLOG(DBG,"RMG: "<< RawPrinter::toString(reg,idpos)<<" has a supp set with no guards");
					DBGLOG(DBG,"RMG: move to next atom in dpos");
					enpos++;
				}

				else {
					emptyrepair=false;
					// go through negative external atoms
					DBGLOG(DBG,"RMG: go through atoms in dneg "<<*dneg);

					while (enneg < enneg_end) {
						ID idneg = reg->ogatoms.getIDByAddress(*enneg);
						if(std::find(dlatnoguard.begin(), dlatnoguard.end(), idneg) != dlatnoguard.end()) {
							DBGLOG(DBG,"RMG: no repair exists: atom in dneg has support sets with no guards");
							repairexists=false;
							break;
						}
						else {
							DBGLOG(DBG,"RMG: consider atom in dneg: "<<RawPrinter::toString(reg,idneg));
							DBGLOG(DBG,"RMG: go through its "<<dlatsupportsets[idneg].size()<<" support sets");
							// go through support sets for the current negative atom

							for (int i=0;i<dlatsupportsets[idneg].size();i++) {
								DBGLOG(DBG,"RMG: consider support set number "<<i);
								// find guards
								DBGLOG(DBG,"RMG: find guard");
								BOOST_FOREACH(ID idns,dlatsupportsets[idneg][i]) {
									if (idns.isGuardAuxiliary()) {
										DBGLOG(DBG,"RMG: found ");
										for (int j=0;j<dlatsupportsets[idpos].size();j++) {
											bool del=false;
											BOOST_FOREACH(ID idps,dlatsupportsets[idpos][j]) {
												if (idps==idns) {
													DBGLOG(DBG,"RMG: found same guard in supp. set for atom in dpos");
													del=true;
												}
											}

											// delete support set which contains guard idns from the set of support sets for the current positve atom
											if (del==true) {
												dlatsupportsets[idpos].erase(dlatsupportsets[idpos].begin() + j);
												DBGLOG(DBG,"RMG: eliminate support set for dpos");
											}
											else {
												DBGLOG(DBG,"RMG: no support set in dpos was found");
											}
										}

										// determine the type of the guard by the size of its tuple
										if (reg->ogatoms.getByAddress(idns.address).tuple.size()==3) {

											DBGLOG(DBG,"RMG: guard is concept");
											//it is a concept, thus delete the assertion from conceptABox
											if (newConceptsABox->getFact(idns.address)==true)
											newConceptsABox->clearFact(idns.address);
										}
										else {
											DBGLOG(DBG,"RMG: guard is role");

											// it is a role, thus delete the assertion from the roleABox
											DLLitePlugin::CachedOntology::RoleAssertion ra;
											ra.first=reg->ogatoms.getByAddress(idns.address).tuple[1];
											ra.second.first=reg->ogatoms.getByAddress(idns.address).tuple[2];
											ra.second.second=reg->ogatoms.getByAddress(idns.address).tuple[3];
											std::vector<DLLitePlugin::CachedOntology::RoleAssertion>::iterator it;
											it = find(newRolesABox.begin(), newRolesABox.end(), ra);
											if (it != newRolesABox.end())
											{
												newRolesABox.erase(remove(newRolesABox.begin(), newRolesABox.end(), ra), newRolesABox.end());

											}
										}
									}
								}
							}
							enneg++;
						}
					}
					if (dlatsupportsets[idpos].empty()) {
						DBGLOG(DBG,"RMG: atom guessed true does not have any support sets anymore");
						repairexists=false;
						break;
					}
				}
				enpos++;
			}
		}
	}

	DBGLOG(DBG, "RMG: (Result) ***** Repair ABox existence ***** " << repairexists);
	if (emptyrepair) {
		DBGLOG(DBG,"RMG: (Result) REPAIR ABOX: empty ABox is a repair");
	}
	else {

		bm::bvector<>::enumerator enma;
		bm::bvector<>::enumerator enma_end;
		enma = newConceptsABox->getStorage().first();
		enma_end = newConceptsABox->getStorage().end();
		DBGLOG(DBG,"RMG: (Result) REPAIR ABOX");
		DBGLOG(DBG,"RMG: (Result) Concept assertions: ");

		while (enma < enma_end) {
			const OrdinaryAtom& oa = reg->ogatoms.getByAddress(*enma);
			//DBGLOG(DBG,"RMG: " << oa);
			DBGLOG(DBG,"RMG: (Result) CONCEPT "<<RawPrinter::toString(reg,reg->ogatoms.getIDByAddress(*enma)));
			enma++;
		}
		DBGLOG(DBG,"RMG: (Result) role assertions");
		BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion rolea, newRolesABox) {
			ID idr = rolea.first;
			ID idcr = rolea.second.first;
			ID iddr = rolea.second.second;
			DBGLOG(DBG,"RMG: (Result) ROLE: "<<RawPrinter::toString(reg,idr)<<"("<<RawPrinter::toString(reg,idcr)<<","<<RawPrinter::toString(reg,iddr)<<")");
		}
	}
	/*
	 if (repairExists) {
	 if (!emptyrepair) {
	 InterpretationPtr extIntr (new Interpretation(reg));
	 // create vector of IDs <ID>
	 std::vector<ID> pconc, nconc, prole, nrole;

	 for (unsigned eaIndex=0; eaIndex<factory.allEatoms.size();eaIndex++) {
	 const ExternalAtom& ea = reg->eatoms.getByID(factory.allEatoms[eaIndex]);

	 pconc.push_back(ea.tuple[1]);
	 nconc.push_back(ea.tuple[2]);
	 prole.push_back(ea.tuple[3]);
	 nrole.push_back(ea.tuple[4]);
	 }

	 bm::bvector<>::enumerator enma;
	 bm::bvector<>::enumerator enma_end;
	 enma = newConceptsABox->getStorage().first();
	 enma_end = newConceptsABox->getStorage().end();

	 DBGLOG(DBG,"RMG: (Result) REPAIR ABOX");
	 DBGLOG(DBG,"RMG: (Result) Concept assertions: ");

	 while (enma < enma_end){
	 const OrdinaryAtom& oa = reg->ogatoms.getByAddress(*enma);
	 //DBGLOG(DBG,"RMG: " << oa);
	 DBGLOG(DBG,"RMG: (Result) CONCEPT "<<RawPrinter::toString(reg,reg->ogatoms.getIDByAddress(*enma)));


	 if (theDLLitePlugin.dlNeg(oa.tuple[1]))
	 BOOST_FOREACH(ID id,nconc) {
	 OrdinaryAtom conc (ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	 conc.tuple.push_back(id);
	 conc.tuple.push_back(oa.tuple[1]);
	 conc.tuple.push_back(oa.tuple[2]);
	 conc.tuple.push_back(ID::termFromInteger(1));
	 ID concID = reg->storeOrdinaryAtom(conc);
	 extIntr->setFact(concID.address);
	 }
	 else

	 BOOST_FOREACH(ID id,pconc) {
	 OrdinaryAtom conc (ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	 conc.tuple.push_back(id);
	 conc.tuple.push_back(oa.tuple[1]);
	 conc.tuple.push_back(oa.tuple[2]);
	 conc.tuple.push_back(ID::termFromInteger(0));
	 ID concID = reg->storeOrdinaryAtom(conc);
	 extIntr->setFact(concID.address);
	 }
	 enma++;
	 }

	 DBGLOG(DBG,"RMG: (Result) role assertions");
	 BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion rolea, newRolesABox){
	 ID idr = rolea.first;
	 ID idcr = rolea.second.first;
	 ID iddr = rolea.second.second;
	 DBGLOG(DBG,"RMG: (Result) ROLE: "<<RawPrinter::toString(reg,idr)<<"("<<RawPrinter::toString(reg,idcr)<<","<<RawPrinter::toString(reg,iddr)<<")");

	 if (theDLLitePlugin.dlNeg(idr))
	 BOOST_FOREACH(ID id,nrole) {
	 OrdinaryAtom role (ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	 role.tuple.push_back(idr);
	 role.tuple.push_back(idcr);
	 role.tuple.push_back(iddr);
	 role.tuple.push_back(ID::termFromInteger(1));
	 ID roleID = reg->storeOrdinaryAtom(role);
	 extIntr->setFact(roleID.address);
	 }
	 else

	 BOOST_FOREACH(ID id,prole) {
	 OrdinaryAtom role (ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
	 role.tuple.push_back(idr);
	 role.tuple.push_back(idcr);
	 role.tuple.push_back(iddr);
	 role.tuple.push_back(ID::termFromInteger(0));
	 ID roleID = reg->storeOrdinaryAtom(role);
	 extIntr->setFact(roleID.address);
	 }

	 }
	 //TODO do same for roles newRolesABox

	 if (!isModel(extIntr))
	 repairExists=false;
	 }
	 }*/
	DBGLOG(DBG, "RMG: (Result) ***** Repair ABox existence ***** " << repairexists);

	return repairexists;
}

bool RepairModelGenerator::isModel(InterpretationConstPtr compatibleSet) {

	// which semantics?
	if (factory.ctx.config.getOption("WellJustified")) {

		// well-justified FLP: fixpoint iteration
		InterpretationPtr fixpoint = welljustifiedSemanticsGetFixpoint(factory.ctx, compatibleSet, grounder->getGroundProgram());
		InterpretationPtr reference = InterpretationPtr(new Interpretation(*compatibleSet));
		factory.gpMask.updateMask();
		factory.gnMask.updateMask();
		reference->getStorage() -= factory.gpMask.mask()->getStorage();
		reference->getStorage() -= factory.gnMask.mask()->getStorage();

		DBGLOG(DBG, "Comparing fixpoint " << *fixpoint << " to reference " << *reference);
		if ((fixpoint->getStorage() & reference->getStorage()).count() == reference->getStorage().count()) {
			DBGLOG(DBG, "Well-Justified FLP Semantics: Pass fixpoint test");
			return true;
		} else {
			DBGLOG(DBG, "Well-Justified FLP Semantics: Fail fixpoint test");
			return false;
		}
	} else {

		// FLP: ensure minimality of the compatible set wrt. the reduct (if necessary)
		if (annotatedGroundProgram.hasHeadCycles() == 0 && annotatedGroundProgram.hasECycles() == 0 && factory.ctx.config.getOption("FLPDecisionCriterionHead") && factory.ctx.config.getOption("FLPDecisionCriterionE")) {
			DBGLOG(DBG, "No head- or e-cycles --> No FLP/UFS check necessary");
			return true;
		} else {
			DBGLOG(DBG, "Head- or e-cycles --> FLP/UFS check necessary");

			// Explicit FLP check
			if (factory.ctx.config.getOption("FLPCheck")) {
				DBGLOG(DBG, "FLP Check");

				// do FLP check (possibly with nogood learning) and add the learned nogoods to the main search
				bool result = isSubsetMinimalFLPModel<GenuineSolver>(compatibleSet, postprocessedInput, factory.ctx, factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr());

				//updateEANogoods(compatibleSet);
				return result;
			}

			// UFS check
			if (factory.ctx.config.getOption("UFSCheck")) {
				DBGLOG(DBG, "UFS Check");
				std::vector<IDAddress> ufs = ufscm->getUnfoundedSet(compatibleSet, std::set<ID>(), factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr());

				//updateEANogoods(compatibleSet);
				if (ufs.size() > 0) {
					DBGLOG(DBG, "Got a UFS");
					if (factory.ctx.config.getOption("UFSLearning")) {
						DBGLOG(DBG, "Learn from UFS");
						Nogood ufsng = ufscm->getLastUFSNogood();
						solver->addNogood(ufsng);
					}
					return false;
				} else {
					return true;
				}
			}

			// no check
			return true;
		}
	}
}

bool RepairModelGenerator::partialUFSCheck(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed) {
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c partialUFSchk");
	return false;
}

bool RepairModelGenerator::isVerified(ID eaAux, InterpretationConstPtr factWasSet) {
	return false;
}
// 1. current interpretation
// 2. which facts are assigned
// 3. which atoms changed their truth value from the revious call (which were reassigned)
// 2,3, can be 0
bool RepairModelGenerator::verifyExternalAtoms(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed) {
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c verifyEAtoms");
	return false;
}

bool RepairModelGenerator::verifyExternalAtom(int eaIndex, InterpretationConstPtr partialInterpretation,
		InterpretationConstPtr factWasSet, InterpretationConstPtr changed) {
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c verifyEAtom");
	return false;
}

const OrdinaryASPProgram& RepairModelGenerator::getGroundProgram() {
	return grounder->getGroundProgram();
}

void RepairModelGenerator::propagate(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed) {

}

}
DLVHEX_NAMESPACE_END

// vi:ts=8:noexpandtab:

