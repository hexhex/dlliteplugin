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

namespace dllite{
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
  if (ctx.config.getOption("LiberalSafety")){
    // add domain predicates for all external atoms which are necessary to establish liberal domain-expansion safety
    // and extract the domain-exploration program from the IDB
    addDomainPredicatesAndCreateDomainExplorationProgram(ci, ctx, idb, deidb, deidbInnerEatoms);
  }

  innerEatoms = ci.innerEatoms;
  
  allEatoms.insert(allEatoms.end(), innerEatoms.begin(), innerEatoms.end());
  allEatoms.insert(allEatoms.end(), outerEatoms.begin(), outerEatoms.end());


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
    if( !factory.outerEatoms.empty() )
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
    }

    // compute extensions of domain predicates and add it to the input
    if (factory.ctx.config.getOption("LiberalSafety")){
      InterpretationConstPtr domPredictaesExtension = computeExtensionOfDomainPredicates(factory.ci, factory.ctx, postprocInput, factory.deidb, factory.deidbInnerEatoms);
      postprocInput->add(*domPredictaesExtension);
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
		DBGLOG(DBG,"RMG: before new line");
		annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.allEatoms);
		DBGLOG(DBG,"RMG: after new line");
	}
	solver = GenuineGroundSolver::getInstance(
		factory.ctx, annotatedGroundProgram,
		// no interleaved threading because guess and check MG will likely not profit from it
		false,
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
      for (int i = 0; i < factory.globalLearnedEANogoods->getNogoodCount(); ++i){
    	  DBGLOG(DBG,"RMG: going through nogoods");
    	  DBGLOG(DBG,"RMG: consider nogood number "<< i<< " which is "<<  factory.globalLearnedEANogoods->getNogood(i));
    	  learnedEANogoods->addNogood(factory.globalLearnedEANogoods->getNogood(i));
      }
      updateEANogoods();
    }
    DBGLOG(DBG,"RMG: RepairModelGenerator constructor is finished");
}

RepairModelGenerator::~RepairModelGenerator(){
	//solver->removePropagator(this);
	DBGLOG(DBG, "Final Statistics:" << std::endl << solver->getStatistics());
}


//called from the core
InterpretationPtr RepairModelGenerator::generateNextModel()
{
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
		//*** model is returned
		DBGLOG(DBG, "Statistics:" << std::endl << solver->getStatistics());
		if( !modelCandidate )
		{
			LOG(DBG,"unsatisfiable -> returning no model");
			return InterpretationPtr();
		}
		DLVHEX_BENCHMARK_REGISTER_AND_COUNT(ssidmodelcandidates, "Candidate compatible sets", 1);
		LOG_SCOPE(DBG,"gM", false);
		LOG(DBG,"got guess model, will do repair check on " << *modelCandidate);
		if (!repairCheck(modelCandidate))
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
		}

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

void RepairModelGenerator::generalizeNogood(Nogood ng){

}


void RepairModelGenerator::learnSupportSets(){
	DBGLOG(DBG,"RMG: learning support sets is started");
	DBGLOG(DBG,"RMG: Number of all eatoms: "<<factory.allEatoms.size());

	// create a map that maps external atom to set of nonground support sets for it
	DBGLOG(DBG,"RMG: create map from id of atom to set of supp sets for it ");
		 std::map<ID,std::vector<Nogood> > dlatsupportsets;


	// we need a separate set of support sets for each external atom
	std::vector<SimpleNogoodContainerPtr> supportSetsOfExternalAtom;

	if (factory.ctx.config.getOption("SupportSets")){
		OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
		program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());

		ID guardPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 0));
		ID guardbarPredicateID = reg->getAuxiliaryConstantSymbol('o', ID(0, 1));
		ID varoID = reg->storeVariableTerm("O");

//		SimpleNogoodContainerPtr potentialSupportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());
//		SimpleNogoodContainerPtr supportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());

		// get ABox predicates, they will be needed to filter out unnuccessary support sets
		DBGLOG(DBG,"RMG: Abox predicates are:");

		DLLitePlugin::CachedOntologyPtr ontology = theDLLitePlugin.prepareOntology(factory.ctx, reg->storeConstantTerm(factory.ctx.getPluginData<DLLitePlugin>().repairOntology));
		std::vector<ID> abp = ontology->AboxPredicates;
		BOOST_FOREACH(ID id,abp) {
				DBGLOG(DBG,"RMG: " <<RawPrinter::toString(reg,id)<<" with "<< id);
		}

		// go through external atoms and add its set of nonground support sets
		for(unsigned eaIndex = 0; eaIndex < factory.allEatoms.size(); ++eaIndex){
			DBGLOG(DBG,"RMG: consider atom "<< RawPrinter::toString(reg,factory.allEatoms[eaIndex]));

			// evaluate the external atom if it provides support sets
			const ExternalAtom& eatom = reg->eatoms.getByID(factory.allEatoms[eaIndex]);

			supportSetsOfExternalAtom.push_back(SimpleNogoodContainerPtr(new SimpleNogoodContainer()));
			if (eatom.getExtSourceProperties().providesSupportSets()){
				DBGLOG(DBG, "RMG: evaluating external atom " << RawPrinter::toString(reg,factory.allEatoms[eaIndex]) << " for support set learning");
				learnSupportSetsForExternalAtom(factory.ctx, eatom, supportSetsOfExternalAtom[eaIndex]);
//				DBGLOG(DBG, "RMG: current number of learnt support sets: " << potentialSupportSets->getNogoodCount());
			}

			DBGLOG(DBG, "RMG: eliminate unneccessary nonground support sets " );
			int s = supportSetsOfExternalAtom[eaIndex]->getNogoodCount();
			for (int i = 0; i < s; i++) {
				bool elim=false;
				const Nogood& ng = supportSetsOfExternalAtom[eaIndex]->getNogood(i);
				DBGLOG(DBG,"RMG: consider support set: "<<ng.getStringRepresentation(reg));
				DBGLOG(DBG,"RMG: is it of size >3? "<<ng.size());
				if (ng.size()>3) {
					DBGLOG(DBG,"RMG: yes");
					elim=true;
					DBGLOG(DBG,"RMG: support set is marked for elimination");
				}else{
					DBGLOG(DBG,"RMG: no");
					BOOST_FOREACH(ID litid, ng) {
						ID newid = reg->onatoms.getIDByAddress(litid.address);
						const OrdinaryAtom& oa = reg->onatoms.getByAddress(litid.address);
						DBGLOG(DBG,"RMG: is " <<RawPrinter::toString(reg,newid)<<" a guard with predicate not occurring in ABox?");
						DBGLOG(DBG,"RMG: check for " <<RawPrinter::toString(reg,oa.tuple[1])<<" with "<<oa.tuple[1]);
						if ((newid.isGuardAuxiliary())&&(std::find(abp.begin(), abp.end(), oa.tuple[1]) == abp.end())) {
							DBGLOG(DBG,"RMG: yes");
							elim = true;
							DBGLOG(DBG,"RMG: support set is marked for elimination");
							break;
						}else{
							DBGLOG(DBG,"RMG: no");
						}
					}
				}
				if (elim) {
					DBGLOG(DBG,"RMG: if current support set is marked, eliminate it");
					supportSetsOfExternalAtom[eaIndex]->removeNogood(ng);
				}
				else {
					DBGLOG(DBG,"RMG: leave this support set");
				}
			}
			DBGLOG(DBG,"RMG: before elimination");
			assert(!!supportSetsOfExternalAtom[eaIndex]);
			supportSetsOfExternalAtom[eaIndex]->defragment();

			// TODO store support sets from potentialSupportSets in the map as follows
			// supportSetsOfExternalAtom[eaIndex]=set od support sets for it
			s = supportSetsOfExternalAtom[eaIndex]->getNogoodCount();
			for (int i = 0; i < s; i++) {
				const Nogood& ng = supportSetsOfExternalAtom[eaIndex]->getNogood(i);

				DBGLOG(DBG, "Checking support set " << ng.getStringRepresentation(reg));

				//			std::vector<ID> s=dlatsupsets[factory.allEatoms[eaIndex]]
				//			if (s for external atom a("Q",O) contains both a guard aux_o("C",X) and ...
				OrdinaryAtom guard(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN);
				guard.tuple.push_back(guardPredicateID);
				guard.tuple.push_back(eatom.tuple[5]);

				ID guardID = ID_FAIL;
				ID cID = ID_FAIL;
				BOOST_FOREACH (ID id, ng){
					const OrdinaryAtom& oatom = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByAddress(id.address) : reg->onatoms.getByAddress(id.address));
					if (oatom.tuple[0] == guardPredicateID){
						cID = oatom.tuple[1];
						guardID = id;
						break;
					}
				}

				bool foundOrdinaryAtom = false;
				if (guardID != ID_FAIL){
					DBGLOG(DBG, "Support set has a guard of the expected type");
					ID guardIDOrig = (guardID.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(guardID.address) : reg->onatoms.getIDByAddress(guardID.address));
					BOOST_FOREACH (ID id, ng){
						ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));
						DBGLOG(DBG, "Checking literal " << RawPrinter::toString(reg, idOrig));
						const OrdinaryAtom& oatom = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getByAddress(id.address) : reg->onatoms.getByAddress(id.address));

						// ... a normal atom aux_p("D",Y)) {
						if (oatom.tuple[0] == eatom.inputs[1]) {
							DBGLOG(DBG, "Literal is a concept addition");
							//				create the following rules:
							//				*	bar_aux_o("C",X):-aux_p("D",Y), aux_o("C",X), n_e_a("Q",O). (neg. repl. of eatom)
							{
								Rule rule(ID::MAINKIND_RULE);
								{
									OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
									headat.tuple.push_back(guardbarPredicateID);
									headat.tuple.push_back(cID);
									headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
									rule.head.push_back(reg->storeOrdinaryAtom(headat));
								}
								rule.body.push_back(ID::posLiteralFromAtom(idOrig));
								rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));
								{
									OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
									repl.tuple.push_back(cID);
									repl.tuple.push_back(varoID);
									rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
								}
								ID ruleID = reg->storeRule(rule);
								DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
								program.idb.push_back(ruleID);
							}
					//				*   supp_e_a("Q",O):-aux_p("D",Y), aux_o("C",X),e_a("Q",O), not bar_aux_o("C",X).
							{
								Rule rule(ID::MAINKIND_RULE);
								{
									OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
									headat.tuple.push_back(cID);
									headat.tuple.push_back(varoID);
									rule.head.push_back(reg->storeOrdinaryAtom(headat));
								}
								rule.body.push_back(ID::posLiteralFromAtom(idOrig));
								rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));
								{
									OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
									repl.tuple.push_back(cID);
									repl.tuple.push_back(varoID);
								}
								{
									OrdinaryAtom notbarat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									notbarat.tuple.push_back(guardbarPredicateID);
									notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
									notbarat.tuple.push_back(cID);
									rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notbarat)));
								}
								ID ruleID = reg->storeRule(rule);
								DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
								program.idb.push_back(ruleID);
							}

					//				* 	:-e_a("Q",O),not supp_e_a("Q",O).
							{
								Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
								{
									OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
									repl.tuple.push_back(cID);
									repl.tuple.push_back(varoID);
									rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
								}
								{
									OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
									notsupp.tuple.push_back(cID);
									notsupp.tuple.push_back(varoID);
									rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
								}
								ID ruleID = reg->storeRule(rule);
								DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
								program.idb.push_back(ruleID);
							}
				//			where bar_aux_o is a fresh predicate and intuitively stands for elimination of C(X) from the ABox
				//			supp_e_a is also a fresh predicate that denotes existence of a support set for a certain atom

				// supp_e_a is retrieved by reg->getAuxiliaryConstantSymbol('o', reg->getIDByAuxiliaryConstantSymbol(factory.allEatoms[eaIndex]))
				// aux_o = guardPredicateID
				// bar_aux_o = guardbarPredicateID


							foundOrdinaryAtom = true;
						}
					}

		//			else if (s for external atom a("Q",O) contains only a guard aux_o("C",X)) }
		//				create the following rules:
					if (!foundOrdinaryAtom){
						DBGLOG(DBG, "Did not find a concept addition");
		//				*	bar_aux_o("C",X):-aux_o("C",X), n_e_a("Q",O). (neg. repl. of eatom)
						{
							Rule rule(ID::MAINKIND_RULE);
							{
								OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::PROPERTY_AUX | (guardIDOrig.isOrdinaryGroundAtom() ? ID::SUBKIND_ATOM_ORDINARYG : ID::SUBKIND_ATOM_ORDINARYN));
								headat.tuple.push_back(guardbarPredicateID);
								headat.tuple.push_back(cID);
								headat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
								rule.head.push_back(reg->storeOrdinaryAtom(headat));
							}
							rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));
							{
								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
								repl.tuple.push_back(cID);
								repl.tuple.push_back(varoID);
								rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
							}
							ID ruleID = reg->storeRule(rule);
							DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
							program.idb.push_back(ruleID);
						}

		//				*   supp_e_a("Q",O):-aux_o("C",X),e_a("Q",O), not bar_aux_o("C",X).
						{
							Rule rule(ID::MAINKIND_RULE);
							{
								OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
								headat.tuple.push_back(cID);
								headat.tuple.push_back(varoID);
								rule.head.push_back(reg->storeOrdinaryAtom(headat));
							}
							rule.body.push_back(ID::posLiteralFromAtom(guardIDOrig));
							{
								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
								repl.tuple.push_back(cID);
								repl.tuple.push_back(varoID);
								rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
							}
							{
								OrdinaryAtom notbarat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								notbarat.tuple.push_back(guardbarPredicateID);
								notbarat.tuple.push_back(reg->lookupOrdinaryAtom(guardIDOrig).tuple[2]);
								notbarat.tuple.push_back(cID);
								rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notbarat)));
							}
							ID ruleID = reg->storeRule(rule);
							DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
							program.idb.push_back(ruleID);
						}
		//				* 	:-e_a("Q",O),not supp_e_a("Q",O).
						{
							Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
							{
								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
								repl.tuple.push_back(cID);
								repl.tuple.push_back(varoID);
								rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
							}
							{
								OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
								notsupp.tuple.push_back(cID);
								notsupp.tuple.push_back(varoID);
								rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
							}
							ID ruleID = reg->storeRule(rule);
							DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
							program.idb.push_back(ruleID);
						}
					}
				}else if (guardID != ID_FAIL){
					DBGLOG(DBG, "Support set does not have a guard of the expected type");
					BOOST_FOREACH (ID id, ng){
						ID idOrig = (id.isOrdinaryGroundAtom() ? reg->ogatoms.getIDByAddress(id.address) : reg->onatoms.getIDByAddress(id.address));
						const OrdinaryAtom& oatom = reg->lookupOrdinaryAtom(idOrig);
						if (oatom.tuple[0] == eatom.inputs[1]) {
							//    create the following rules:

							//    *   :-aux_p("D",Y), n_e_a("Q",O). (neg. repl. of eatom)
							Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
							rule.body.push_back(idOrig);
							{
								OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
								repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('n', eatom.predicate));
								repl.tuple.push_back(cID);
								repl.tuple.push_back(varoID);
								rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
							}
							ID ruleID = reg->storeRule(rule);
							DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
							program.idb.push_back(ruleID);

							//    *   supp_e_a("Q",O):-e_a("Q",O), aux_p("D",Y).
							{
								Rule rule(ID::MAINKIND_RULE);
								{
									OrdinaryAtom headat(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									headat.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
									headat.tuple.push_back(cID);
									headat.tuple.push_back(varoID);
									rule.head.push_back(reg->storeOrdinaryAtom(headat));
								}
								rule.body.push_back(ID::posLiteralFromAtom(idOrig));
								{
									OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
									repl.tuple.push_back(cID);
									repl.tuple.push_back(varoID);
								}
								rule.body.push_back(idOrig);
								ID ruleID = reg->storeRule(rule);
								DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
								program.idb.push_back(ruleID);
							}

							//    *   :-e_a("Q",O),not supp_e_a("Q",O).
							{
								Rule rule(ID::MAINKIND_RULE | ID::SUBKIND_RULE_CONSTRAINT);
								{
									OrdinaryAtom repl(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									repl.tuple.push_back(reg->getAuxiliaryConstantSymbol('r', eatom.predicate));
									repl.tuple.push_back(cID);
									repl.tuple.push_back(varoID);
									rule.body.push_back(ID::posLiteralFromAtom(reg->storeOrdinaryAtom(repl)));
								}
								{
									OrdinaryAtom notsupp(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYN | ID::PROPERTY_AUX);
									notsupp.tuple.push_back(reg->getAuxiliaryConstantSymbol('o', factory.allEatoms[eaIndex]));
									notsupp.tuple.push_back(cID);
									notsupp.tuple.push_back(varoID);
									rule.body.push_back(ID::nafLiteralFromAtom(reg->storeOrdinaryAtom(notsupp)));
								}
								ID ruleID = reg->storeRule(rule);
								DBGLOG(DBG, "Adding the following rule: " << RawPrinter::toString(reg, ruleID));
								program.idb.push_back(ruleID);
							}
							//   }
						}
					}
				}
			}
		}

		DBGLOG(DBG, "Adding Abox");
		InterpretationPtr edb(new Interpretation(reg));
		edb->add(*program.edb);
		program.edb = edb;

		// add ontology ABox in form of facts aux_o("D",c)
		edb->add(*ontology->conceptAssertions);

		// (accordingly aux_o("R",c1,c2)).
		BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion ra, ontology->roleAssertions){
			OrdinaryAtom roleAssertion(ID::MAINKIND_ATOM | ID::SUBKIND_ATOM_ORDINARYG | ID::PROPERTY_AUX);
			roleAssertion.tuple.push_back(guardPredicateID);
			roleAssertion.tuple.push_back(ra.first);
			roleAssertion.tuple.push_back(ra.second.first);
			roleAssertion.tuple.push_back(ra.second.second);
			edb->setFact(reg->storeOrdinaryAtom(roleAssertion).address);
		}


		// ground the program and evaluate it
		// get the results, filter them out with respect to only relevant predicates (all apart from aux_o, replacement atoms)
		grounder = GenuineGrounder::getInstance(factory.ctx, program);
		annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.innerEatoms);

		solver = GenuineGroundSolver::getInstance(
			factory.ctx, annotatedGroundProgram,
			// no interleaved threading because guess and check MG will likely not profit from it
			false,
			// do the UFS check for disjunctions only if we don't do
			// a minimality check in this class;
			// this will not find unfounded sets due to external sources,
			// but at least unfounded sets due to disjunctions
			!factory.ctx.config.getOption("FLPCheck") && !factory.ctx.config.getOption("UFSCheck"));
		nogoodGrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), learnedEANogoods, learnedEANogoods, annotatedGroundProgram));

	}

#if 0
		DLVHEX_BENCHMARK_REGISTER(sidnongroundpsupportsets, "nonground potential supportsets");
		DLVHEX_BENCHMARK_COUNT(sidnongroundpsupportsets, potentialSupportSets->getNogoodCount());

		// ground the support sets exhaustively
		DBGLOG(DBG, "RMG: start grounding supports sets");
		NogoodGrounderPtr nogoodgrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), potentialSupportSets, potentialSupportSets, annotatedGroundProgram));

		int nc = 0;
		while (nc < potentialSupportSets->getNogoodCount()){
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
		for (int i = 0; i < potentialSupportSets->getNogoodCount(); ++i){
			const Nogood& ng = potentialSupportSets->getNogood(i);
			DBGLOG(DBG, "RMG: current support set "<<i<<" namely " << ng.getStringRepresentation(reg));
			DBGLOG(DBG, "RMG: is it ground?");
			if (ng.isGround()) {
			DBGLOG(DBG, "RMG: yes");
				// determine whether it has a guard
				DBGLOG(DBG, "RMG: does it have guard atom?");
				isGuard=false;
				BOOST_FOREACH (ID lit, ng){
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

				BOOST_FOREACH (ID lit, ng){
					if (reg->ogatoms.getIDByAddress(lit.address).isExternalAuxiliary()){
						if (eaAux != ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains multiple external literals");
						eaAux = lit;
					}
				}
				if (eaAux == ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains no external literals");

					if (annotatedGroundProgram.mapsAux(eaAux.address)){
						DBGLOG(DBG, "RMG: evaluating guards (if there are any) of " << ng.getStringRepresentation(reg));
						keep = true;
						Nogood ng2 = ng;
						reg->eatoms.getByID(annotatedGroundProgram.getAuxToEA(eaAux.address)[0]).pluginAtom->guardSupportSet(keep, ng2, eaAux);
						if (keep){
				#ifdef DEBUG
							// ng2 must be a subset of ng and still a valid support set
							ID aux = ID_FAIL;
							BOOST_FOREACH (ID id, ng2){
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
		 annotatedGroundProgram.setCompleteSupportSetsForVerification(learnedEANogoods);
	}
#endif

}



void RepairModelGenerator::updateEANogoods(
	InterpretationConstPtr compatibleSet,
	InterpretationConstPtr factWasSet,
	InterpretationConstPtr changed){

	// generalize ground nogoods to nonground ones
	if (factory.ctx.config.getOption("ExternalLeaok, now its clearrningGeneralize")){
		int max = learnedEANogoods->getNogoodCount();
		for (int i = learnedEANogoodsTransferredIndex; i < max; ++i){
			generalizeNogood(learnedEANogoods->getNogood(i));
		}
	}

	// instantiate nonground nogoods
	if (factory.ctx.config.getOption("NongroundNogoodInstantiation")){
		nogoodGrounder->update(compatibleSet, factWasSet, changed);
	}

	// transfer nogoods to the solver
	DLVHEX_BENCHMARK_REGISTER_AND_COUNT(sidcompatiblesets, "Learned EA-Nogoods", learnedEANogoods->getNogoodCount() - learnedEANogoodsTransferredIndex);
	for (int i = learnedEANogoodsTransferredIndex; i < learnedEANogoods->getNogoodCount(); ++i){
		const Nogood& ng = learnedEANogoods->getNogood(i);
		if (factory.ctx.config.getOption("PrintLearnedNogoods")){
		  	// we cannot use i==1 because of learnedEANogoods.clear() below in this function
		 	static bool first = true; 
			if( first )
			{
			  if (factory.ctx.config.getOption("GenuineSolver") >= 3){
				  LOG(DBG, "( NOTE: With clasp backend, learned nogoods become effective with a delay because of multithreading! )");
			  }else{
				  LOG(DBG, "( NOTE: With i-backend, learned nogoods become effective AFTER the next model was printed ! )");
			  }
			  first = false;
			}
			LOG(DBG,"learned nogood " << ng.getStringRepresentation(reg));
		}
		if (ng.isGround()){
			solver->addNogood(ng);
		}else{
			// keep nonground nogoods beyond the lifespan of this model generator
			factory.globalLearnedEANogoods->addNogood(ng);
		}
	}

	// for encoding-based UFS checkers and explicit FLP checks, we need to keep learned nogoods (otherwise future UFS searches will not be able to use them)
	// for assumption-based UFS checkers we can delete them as soon as nogoods were added both to the main search and to the UFS search
	if (factory.ctx.config.getOption("UFSCheckAssumptionBased") ||
	    (annotatedGroundProgram.hasECycles() == 0 && factory.ctx.config.getOption("FLPDecisionCriterionE"))){
		ufscm->learnNogoodsFromMainSearch(true);
	//	nogoodGrounder->resetWatched(learnedEANogoods);
		learnedEANogoods->clear();
	}else{
		learnedEANogoods->forgetLeastFrequentlyAdded();
	}
	learnedEANogoodsTransferredIndex = learnedEANogoods->getNogoodCount();
}



bool RepairModelGenerator::repairCheck(InterpretationConstPtr modelCandidate){
	
	DBGLOG(DBG,"RMG: repair check is started:");
	DBGLOG(DBG,"RMG: (Result) current model candidate is: "<< *modelCandidate);

	// repair exists is a flag that witnesses the ABox repair existence 	
	bool repairexists;
	bool emptyrepair;
	repairexists = true;
	int ngCount;

	DBGLOG(DBG,"RMG: number of all external atoms: " << factory.allEatoms.size());

	// We divide all external atoms into two groups:
		// group dpos: those that were guessed true in modelCandidate;
		// group npos: and those that were guessed false in modelCandidate.

	InterpretationPtr dpos(new Interpretation(reg));
	InterpretationPtr dneg(new Interpretation(reg));

	// Go through all atoms in alleatoms
	// and evaluate them
	// mask stores all relevant atoms for external atom with index eaindex
	for (unsigned eaIndex=0; eaIndex<factory.allEatoms.size();eaIndex++){
		DBGLOG(DBG,"RMG: consider atom "<< eaIndex<<", namely "<< RawPrinter::toString(reg,factory.allEatoms[eaIndex]));
		annotatedGroundProgram.getEAMask(eaIndex)->updateMask();

		const InterpretationConstPtr& mask = annotatedGroundProgram.getEAMask(eaIndex)->mask();

		DBGLOG(DBG,"RMG: mask is created "<< *mask);

		bm::bvector<>::enumerator enm;
		bm::bvector<>::enumerator enm_end;
		// make sure that ALL input auxiliary atoms are true, otherwise we might miss some output atoms and consider true output atoms wrongly as unfounded
		// thus we need the following:
		InterpretationPtr evalIntr(new Interpretation(reg));

		if (!factory.ctx.config.getOption("IncludeAuxInputInAuxiliaries")){
				// clone and extend
				evalIntr->getStorage() |= annotatedGroundProgram.getEAMask(eaIndex)->getAuxInputMask()->getStorage();
		}


		// analyze the current external atom

		enm = mask->getStorage().first();
	    enm_end = mask->getStorage().end();

		DBGLOG(DBG,"RMG: go through elements of mask ");
		while (enm < enm_end){
			ID id = reg->ogatoms.getIDByAddress(*enm);
			DBGLOG(DBG,"RMG: atom "<<RawPrinter::toString(reg,id));

			if (id.isExternalAuxiliary() && !id.isExternalInputAuxiliary()){
				// it is an external atom replacement, now check if it is positive or negative
				DBGLOG(DBG,"RMG: replacement");
				if (reg->isPositiveExternalAtomAuxiliaryAtom(id)){
					DBGLOG(DBG,"RMG: positive");
					// add it to dpos
					if (modelCandidate->getFact(*enm)) {
						DBGLOG(DBG,"RMG: guessed true");
						DBGLOG(DBG,"RMG: add " << RawPrinter::toString(reg,id) << " to dpos ");
						dpos->setFact(*enm);
					}
					else DBGLOG(DBG,"RMG: guessed false");

				}
				else { 	DBGLOG(DBG,"RMG: negative"); // it is negative
					if (modelCandidate->getFact(*enm)) {
						DBGLOG(DBG,"RMG: guessed true");
						ID inverse = reg->swapExternalAtomAuxiliaryAtom(id);
						DBGLOG(DBG,"RMG: add atom " << RawPrinter::toString(reg,inverse) << " to set dneg ");
						dneg->setFact(reg->swapExternalAtomAuxiliaryAtom(id).address);
					}
					else DBGLOG(DBG,"RMG: guessed false");
					// the following assertion is not needed, but should be added to make the implementation more rebust
					// (it might help to find programming errors later on)
				}
				//assert(reg->isNegativeExternalAtomAuxiliaryAtom(id) && "replacement atom is neither positive nor negative");
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
			else if (newid.isGuardAuxiliary()) {  //guard atom
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
					while (enneg < enneg_end){
						ID idneg=reg->ogatoms.getIDByAddress(*enneg);
						if(std::find(dlatnoguard.begin(), dlatnoguard.end(), idneg) != dlatnoguard.end()) {
							DBGLOG(DBG,"RMG: no repair exists: atom in dneg has support sets with no guards");
								repairexists=false;
								break;
						}
						else  {
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
					while (enpos < enpos_end){
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
					while (enpos < enpos_end){
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


							while (enneg < enneg_end){
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
			if (emptyrepair) 	{
				DBGLOG(DBG,"RMG: (Result) REPAIR ABOX: empty ABox is a repair");
			}
			else {

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
							enma++;
						}
						DBGLOG(DBG,"RMG: (Result) role assertions");
						BOOST_FOREACH (DLLitePlugin::CachedOntology::RoleAssertion rolea, newRolesABox){
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

bool RepairModelGenerator::isModel(InterpretationConstPtr compatibleSet){

	// which semantics?
	if (factory.ctx.config.getOption("WellJustified")){

		// well-justified FLP: fixpoint iteration
		InterpretationPtr fixpoint = welljustifiedSemanticsGetFixpoint(factory.ctx, compatibleSet, grounder->getGroundProgram());
		InterpretationPtr reference = InterpretationPtr(new Interpretation(*compatibleSet));
		factory.gpMask.updateMask();
		factory.gnMask.updateMask();
		reference->getStorage() -= factory.gpMask.mask()->getStorage();
		reference->getStorage() -= factory.gnMask.mask()->getStorage();

		DBGLOG(DBG, "Comparing fixpoint " << *fixpoint << " to reference " << *reference);
		if ((fixpoint->getStorage() & reference->getStorage()).count() == reference->getStorage().count()){
			DBGLOG(DBG, "Well-Justified FLP Semantics: Pass fixpoint test");
			return true;
		}else{
			DBGLOG(DBG, "Well-Justified FLP Semantics: Fail fixpoint test");
			return false;
		}
	}else{

		// FLP: ensure minimality of the compatible set wrt. the reduct (if necessary)
		if (annotatedGroundProgram.hasHeadCycles() == 0 && annotatedGroundProgram.hasECycles() == 0 && factory.ctx.config.getOption("FLPDecisionCriterionHead") && factory.ctx.config.getOption("FLPDecisionCriterionE")){
			DBGLOG(DBG, "No head- or e-cycles --> No FLP/UFS check necessary");
			return true;
		}else{
			DBGLOG(DBG, "Head- or e-cycles --> FLP/UFS check necessary");

			// Explicit FLP check
			if (factory.ctx.config.getOption("FLPCheck")){
				DBGLOG(DBG, "FLP Check");

				// do FLP check (possibly with nogood learning) and add the learned nogoods to the main search
				bool result = isSubsetMinimalFLPModel<GenuineSolver>(compatibleSet, postprocessedInput, factory.ctx, factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr());

				//updateEANogoods(compatibleSet);
				return result;
			}

			// UFS check
			if (factory.ctx.config.getOption("UFSCheck")){
				DBGLOG(DBG, "UFS Check");
				std::vector<IDAddress> ufs = ufscm->getUnfoundedSet(compatibleSet, std::set<ID>(), factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr());

				//updateEANogoods(compatibleSet);
				if (ufs.size() > 0){
					DBGLOG(DBG, "Got a UFS");
					if (factory.ctx.config.getOption("UFSLearning")){
						DBGLOG(DBG, "Learn from UFS");
						Nogood ufsng = ufscm->getLastUFSNogood();
						solver->addNogood(ufsng);
					}
					return false;
				}else{
					return true;
				}
			}

			// no check
			return true;
		}
	}
}

bool RepairModelGenerator::partialUFSCheck(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed){
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c partialUFSchk");
	return false;
}

bool RepairModelGenerator::isVerified(ID eaAux, InterpretationConstPtr factWasSet){
	return false;
}
// 1. current interpretation
// 2. which facts are assigned
// 3. which atoms changed their truth value from the revious call (which were reassigned)
// 2,3, can be 0
bool RepairModelGenerator::verifyExternalAtoms(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed){
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c verifyEAtoms");
    return false;
}

bool RepairModelGenerator::verifyExternalAtom(int eaIndex, InterpretationConstPtr partialInterpretation,
	       	InterpretationConstPtr factWasSet, InterpretationConstPtr changed){
	DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid, "genuine g&c verifyEAtom");
    return false;
}


const OrdinaryASPProgram& RepairModelGenerator::getGroundProgram(){
	return grounder->getGroundProgram();
}

void RepairModelGenerator::propagate(InterpretationConstPtr partialInterpretation, InterpretationConstPtr factWasSet, InterpretationConstPtr changed){

}

}
DLVHEX_NAMESPACE_END

// vi:ts=8:noexpandtab:
