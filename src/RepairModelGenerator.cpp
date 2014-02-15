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
	DBGLOG(DBG,"evaluating guessing program");
	// no mask
	OrdinaryASPProgram program(reg, factory.xidb, postprocessedInput, factory.ctx.maxint);
	// append gidb to xidb
	program.idb.insert(program.idb.end(), factory.gidb.begin(), factory.gidb.end());

	{
		DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sidhexground, "HEX grounder time");
		grounder = GenuineGrounder::getInstance(factory.ctx, program);
		annotatedGroundProgram = AnnotatedGroundProgram(factory.ctx, grounder->getGroundProgram(), factory.innerEatoms);
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
 DBGLOG(DBG,"RMG: after learnSupportSets method is finished");

    // initialize UFS checker
    //   Concerning the last parameter, note that clasp backend uses choice rules for implementing disjunctions:
    //   this must be regarded in UFS checking (see examples/trickyufs.hex)

 	ufscm = UnfoundedSetCheckerManagerPtr(new UnfoundedSetCheckerManager(*this, factory.ctx, annotatedGroundProgram, factory.ctx.config.getOption("GenuineSolver") >= 3, factory.ctx.config.getOption("ExternalLearning") ? learnedEANogoods : SimpleNogoodContainerPtr()));
 	DBGLOG(DBG,"RMG: after creation of unfoundedset managerchecker");

    // overtake nogoods from the factory
    {
   	  DBGLOG(DBG,"RMG: before DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,genuine g&c init nogoods)");
      DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,"genuine g&c init nogoods");
      DBGLOG(DBG,"RMG: after DLVHEX_BENCHMARK_REGISTER_AND_SCOPE(sid,genuine g&c init nogoods)");
      for (int i = 0; i < factory.globalLearnedEANogoods->getNogoodCount(); ++i){
    	  DBGLOG(DBG,"RMG: going through nogoods");
    	  DBGLOG(DBG,"RMG: consider nogood number "<< i<< " which is "<<  factory.globalLearnedEANogoods->getNogood(i));
    	  learnedEANogoods->addNogood(factory.globalLearnedEANogoods->getNogood(i));
      }
      //updateEANogoods();
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
		// getnextmodel calls propogate method 
		DBGLOG(DBG,"a model candidate is obtained: " << *modelCandidate);
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
			LOG(DBG,"No repair that turns a model candidate into a compatible set was found");
		}

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
		return modelCandidate;
	}while(true);
}

void RepairModelGenerator::generalizeNogood(Nogood ng){

}

void RepairModelGenerator::learnSupportSets(){
	DBGLOG(DBG,"RMG: learning support sets is started")
	if (factory.ctx.config.getOption("SupportSets")){
	DBGLOG(DBG,"RMG: option supportsets is recognized");
		SimpleNogoodContainerPtr potentialSupportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());
		factory.supportSets = SimpleNogoodContainerPtr(new SimpleNogoodContainer());

		DBGLOG(DBG,"RMG: Number of innereatoms: "<<factory.innerEatoms.size());
		DBGLOG(DBG,"RMG: Number of outerereatoms: "<<factory.outerEatoms.size());
		DBGLOG(DBG,"RMG: Number of all eatoms: "<<factory.allEatoms.size());
				
		
		// learn support sets for all external atoms
		for(unsigned eaIndex = 0; eaIndex < factory.allEatoms.size(); ++eaIndex){
			DBGLOG(DBG,"RMG: start going through all external atoms and learning potential support sets for them");
			const ExternalAtom& eatom = reg->eatoms.getByID(factory.allEatoms[eaIndex]);
			if (eatom.getExtSourceProperties().providesSupportSets()){
				DBGLOG(DBG, "RMG: evaluating external atom " << factory.allEatoms[eaIndex] << " for support set learning");
				learnSupportSetsForExternalAtom(factory.ctx, eatom, potentialSupportSets);
			}
			DBGLOG(DBG,"RMG: finished going through all external atoms and potential support sets were learnt for them");
		}


		DLVHEX_BENCHMARK_REGISTER(sidnongroundpsupportsets, "nonground potential supportsets");
		DLVHEX_BENCHMARK_COUNT(sidnongroundpsupportsets, potentialSupportSets->getNogoodCount());

		// ground the support sets exhaustively
		DBGLOG(DBG,"RMG: grounding of potential support sets is started ");		
		NogoodGrounderPtr nogoodgrounder = NogoodGrounderPtr(new ImmediateNogoodGrounder(factory.ctx.registry(), potentialSupportSets, potentialSupportSets, annotatedGroundProgram));

		int nc = 0;
		while (nc < potentialSupportSets->getNogoodCount()){
			nc = potentialSupportSets->getNogoodCount();
			nogoodgrounder->update();
		}
                DLVHEX_BENCHMARK_REGISTER(sidgroundpsupportsets, "ground potential supportsets");
                DLVHEX_BENCHMARK_COUNT(sidgroundpsupportsets, supportSets->getNogoodCount());

		// some support sets are also learned nogoods
		bool keep;
		bool isGuard;
		DBGLOG(DBG,"RMG: number of potential ground support sets is: " << potentialSupportSets->getNogoodCount());			DBGLOG(DBG,"RMG: start going through potential ground support sets and pick those that are without a guard, they will be added as nogoods");		
		for (int i = 0; i < potentialSupportSets->getNogoodCount(); ++i) {
			const Nogood& ng = potentialSupportSets->getNogood(i);
			isGuard=false;
			DBGLOG(DBG,"RMG: consider potential support set viewed as nogood " << ng.getStringRepresentation(reg) << " with id " << ng);		
			// Check if it is a nogood without a guard, use flag isGuard for that
			DBGLOG(DBG,"RMG: checking guards presence");	
		
			BOOST_FOREACH (ID lit, ng){
				// for each literal we check whether it is ground or nonground
				// and then for ground and nonround literals we seperately identify whether it is a guard or not
				if (lit.isOrdinaryGroundAtom()) {
					DBGLOG(DBG,"RMG: considered literal " << lit << " is ground. ");
					ID litID = reg->ogatoms.getIDByAddress(lit.address);
					DBGLOG(DBG,"RMG: ID of the considered literal is " << litID);
					// check if it is not a guard atom
					if (litID.isAuxiliary()){
						DBGLOG(DBG,"RMG: " << litID <<" is auxiliary.");
						const OrdinaryAtom& possibleGuardAtom = reg->lookupOrdinaryAtom(lit);
						DBGLOG(DBG,"RMG: possible guard atom is " << possibleGuardAtom<< " tuple[0] is "<< possibleGuardAtom.tuple[0]);
						if (possibleGuardAtom.tuple[0] != theDLLitePlugin.guardPredicateID) {
								DBGLOG(DBG,"RMG: "<< possibleGuardAtom.tuple[0] << "!=" << theDLLitePlugin.guardPredicateID);
								continue;
						}
						else {
						    	isGuard=true;
								DBGLOG(DBG,"RMG: support set viewed as nogood "<< ng.getStringRepresentation(reg) <<" contains a guard " << possibleGuardAtom.tuple[0]);
							    break;							}
					}
				}
				else {
					DBGLOG(DBG,"RMG: considered literal " << lit << " is nonground. ");
					ID litID = reg->onatoms.getIDByAddress(lit.address);
					DBGLOG(DBG,"RMG: ID of the considered literal is " << litID);
					// check if it is not a guard atom
					if (litID.isAuxiliary()){
						const OrdinaryAtom& possibleGuardAtom = reg->lookupOrdinaryAtom(lit);
						DBGLOG(DBG,"RMG: possible guard atom is " << possibleGuardAtom<< " tuple[0] is "<< possibleGuardAtom.tuple[0]);
						if (possibleGuardAtom.tuple[0] != theDLLitePlugin.guardPredicateID) {
							DBGLOG(DBG,"RMG: theDLLitePlugin.guardPredicateIn is "  << theDLLitePlugin.guardPredicateID);
							continue;
						}
						else {
						  	isGuard=true;
							DBGLOG(DBG,"RMG: support set viewed as nogood "<< ng.getStringRepresentation(reg) <<" contains a guard " << possibleGuardAtom.tuple[0]);
						}
					}
				}
			}
			DBGLOG(DBG,"RMG: ******************Finished analysing the current ng************************ ");
			if (ng.isGround()){
									// determine the external atom replacement in ng
				ID eaAux = ID_FAIL;
				BOOST_FOREACH (ID lit, ng){
						if (reg->ogatoms.getIDByAddress(lit.address).isExternalAuxiliary()){
							if (eaAux != ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains multiple external literals");
							eaAux = lit;
						}
				}
				if (eaAux == ID_FAIL) throw GeneralError("Set " + ng.getStringRepresentation(reg) + " is not a valid support set because it contains no external literals");

				if (isGuard==false) {
					DBGLOG(DBG, "RMG: adding the followig nogood: " << ng.getStringRepresentation(reg));
					learnedEANogoods->addNogood(ng);
					}
				}
			}	
				
		}

                DLVHEX_BENCHMARK_REGISTER(sidgroundsupportsets, "final ground supportsets");
                DLVHEX_BENCHMARK_COUNT(sidgroundsupportsets, supportSets->getNogoodCount());

	    DBGLOG(DBG, "RMG: learnSupportSets method is finished");

}

void RepairModelGenerator::updateEANogoods(
	InterpretationConstPtr compatibleSet,
	InterpretationConstPtr factWasSet,
	InterpretationConstPtr changed){

	// generalize ground nogoods to nonground ones
	if (factory.ctx.config.getOption("ExternalLearningGeneralize")){
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
	
	DBGLOG(DBG,"Repair check is started:");
	// repair exists is a flag that witnesses the ABox repair existence 	
	bool repairExists;
	int ngCount;
	repairExists = true;

	DBGLOG(DBG,"RMG: Number of inner external atoms: " << factory.innerEatoms.size());
	DBGLOG(DBG,"RMG: Number of outer external atoms: " << factory.outerEatoms.size());
	DBGLOG(DBG,"RMG: Number of all external atoms: " << factory.allEatoms.size());

	// We divide all external atoms into two groups:
		// group dpos: those that were guessed true in modelCandidate;
		// group npos: and those that were guessed false in modelCandidate.

	InterpretationPtr dpos(new Interpretation(reg));
	InterpretationPtr dneg(new Interpretation(reg));

		
	/*for (int eaIndex = 0; eaIndex < factory.outerEatoms.size(); ++eaIndex){
		DBGLOG(DBG,"Considered external atom: " << factory.outerEatoms[eaIndex]);
		DBGLOG(DBG,"It is in fact: " << factory.outerEatoms[eaIndex]);
	}*/
	
//	bm::bvector<>::enumerator en = modelCandidate->getStorage().first();
//	bm::bvector<>::enumerator en_end =  modelCandidate->getStorage().end();
	
	
	/*while (en < en_end){
		ID id = reg->ogatoms.getIDByAddress(*en);
		if (id.isExternalAuxiliary() && !id.isExternalInputAuxiliary()){
		// it is an external atom replacement, now check if it is positive or negative
			if (reg->isPositiveExternalAtomAuxiliaryAtom(id)){
				DBGLOG(DBG,"RMG: Atom " << id << " is positive");
			}else{
				DBGLOG(DBG,"RMG: Atom " << id << " is negative");
				// negative

		// the following assertion is not needed, but should be added to make the implementation more rebust
		// (it might help to find programming errors later on)
		//assert(reg->isNegativeExternalAtomAuxiliaryAtom(id) && "replacement atom is neither positive nor negative");
			}
		}
		en++;
	}*/

	// Go through all atoms in alleatoms
	// and evaluate them
	// mask stores all relevant atoms for external atom with index eaindex


	for (int eaIndex=0; eaIndex<factory.allEatoms.size();eaIndex++){
		DBGLOG(DBG,"RMG: consider external atom number "<< eaIndex);
		const InterpretationConstPtr& mask = annotatedGroundProgram.getEAMask(eaIndex)->mask();
		DBGLOG(DBG,"RMG: interpretation mask is created "<< mask);
		// make sure that ALL input auxiliary atoms are true, otherwise we might miss some output atoms and consider true output atoms wrongly as unfounded
		// thus we need the following:
		InterpretationPtr evalIntr(new Interpretation(reg));

		if (!factory.ctx.config.getOption("IncludeAuxInputInAuxiliaries")){
				// clone and extend
				evalIntr->getStorage() |= annotatedGroundProgram.getEAMask(eaIndex)->getAuxInputMask()->getStorage();
		}

		// call back
		// interptretation for storing the result true vectors for external atoms is created
		DBGLOG(DBG,"RMG: interpretation for storing result true values of external atoms is created "<< mask);
		InterpretationPtr newint(new Interpretation(reg));
		IntegrateExternalAnswerIntoInterpretationCB cb(newint);

		// evaluate external atom
		DBGLOG(DBG,"RMG: evaluation of external atom number "<< eaIndex << ", namely "<< factory.allEatoms[eaIndex] << " is started");
		evaluateExternalAtom(factory.ctx, reg->eatoms.getByID(factory.allEatoms[eaIndex]), evalIntr, cb);
		bm::bvector<>::enumerator enm = mask->getStorage().first();
		bm::bvector<>::enumerator enm_end =  mask->getStorage().end();

		while (enm < enm_end){
			ID id = reg->ogatoms.getIDByAddress(*enm);
			if (id.isExternalAuxiliary() && !id.isExternalInputAuxiliary()){
			// it is an external atom replacement, now check if it is positive or negative
				if (reg->isPositiveExternalAtomAuxiliaryAtom(id)){
					DBGLOG(DBG,"RMG: Atom " << id << " is positive");
					if (newint->getFact(*enm)) {
						// add the current atom to the set of positive external atoms D1.
						DBGLOG(DBG,"RMG: add atom with id " << id << " to the set of those that are guessed true ");
						dpos->setFact(*enm);
					}
					}
				}else {
					DBGLOG(DBG,"RMG: Atom " << id << " is negative,"<<" indeed, reg->isPositiveExternalAtomAuxiliaryAtom(id) is "<<reg->isPositiveExternalAtomAuxiliaryAtom(id));
					if (newint->getFact(*enm)) {
						// add it to the set of negative external atoms D2.
						DBGLOG(DBG,"RMG: add atom with id " << id << " to the set of those that are guessed false ");
						dneg->setFact(*enm);
					}
					DBGLOG(DBG,"RMG: finished adding negative external atom to the set dneg");
				}
			DBGLOG(DBG,"RMG: finished analysing the current external atom");
			// the following assertion is not needed, but should be added to make the implementation more rebust
			// (it might help to find programming errors later on)
					assert(reg->isNegativeExternalAtomAuxiliaryAtom(id) && "replacement atom is neither positive nor negative");
			DBGLOG(DBG,"RMG: current enm is" << *enm);
			enm++;
			DBGLOG(DBG,"RMG: enm is incremented and now it is "<< *enm);
		}
		DBGLOG(DBG,"RMG: finished evluating external atom number "<< eaIndex);
	}
	DBGLOG(DBG,"RMG: got out of the loop that goes through all external atoms");


	// It is ensured by apriori defined nogoods that all support sets for external atoms from dneg
	// are dependent on the ABox (their guards are nonempty)

	// Set create a temporary repairABox and store there original ABox
	DBGLOG(DBG,"RMG: start creating a temporary ABox");
	DLLitePlugin::CachedOntologyPtr newOntology = theDLLitePlugin.prepareOntology(factory.ctx, reg->storeConstantTerm(factory.ctx.getPluginData<DLLitePlugin>().repairOntology));
	InterpretationPtr newConceptsABoxPtr = newOntology->conceptAssertions;
	InterpretationPtr newConceptsABox(new Interpretation(reg));
	newConceptsABox->add(*newConceptsABoxPtr);
	std::vector<DLLitePlugin::CachedOntology::RoleAssertion> newRolesABox = newOntology->roleAssertions;
	DBGLOG(DBG,"RMG: a temporary ABox is created");
	DBGLOG(DBG,"RMG: start creating a map for external atoms IDs");
	// create a map that maps id of external atoms to vector of its support sets 
	 std::map<ID,std::vector<Nogood> > dlatsupportsets;
	 // set of IDs of DL-atoms which have support sets which do not contain any guards
	 std::vector<ID> dlatnoguard;
	 // go through all stored nogoods
	  for (int i = 0; i<factory.supportSets->getNogoodCount();i++) {
		// keep is a flag that identifies whether a certain support set for a DL-atom should be kept and added to the map
		bool keep = true;
		bool hasAuxiliary = false;
		ID currentExternalId;
		BOOST_FOREACH(ID id,factory.supportSets->getNogood(i)) {
			// distinct between ordinary atoms, replacement atoms and the guards
			ID newid = reg->ogatoms.getIDByAddress(id.address);
			// if the atom is replacement atom, then store its id in currentExternalId
			if (newid.isExternalAuxiliary()) {
				currentExternalId = newid;
				// if the current replacement atom is not already present in the map then add it to the map
				if (dlatsupportsets.count(currentExternalId)==0) {
					std::vector<Nogood> supset;
					dlatsupportsets[currentExternalId] = supset;
				}
			}
			// if the current atom is a guard then do nothing
			else if (newid.isGuardAuxiliary()) {  //guard atom
				hasAuxiliary = true;
			}
			// if the current atom is an ordinary atom then check whether it is true in the current model and if it is then
			else { // ordinary input atom
				if (modelCandidate->getFact(newid.address)==newid.isNaf()) keep = false;
			}
		}
		if (keep) {
			dlatsupportsets[currentExternalId].push_back(factory.supportSets->getNogood(i));
			if (hasAuxiliary == false)
			{
				dlatnoguard.push_back(currentExternalId);
			}
		}
	}

	bm::bvector<>::enumerator enpos = dpos->getStorage().first();
	bm::bvector<>::enumerator enpos_end = dpos->getStorage().end();
	bm::bvector<>::enumerator enneg = dneg->getStorage().first();
	bm::bvector<>::enumerator enneg_end = dneg->getStorage().end();

			// for (each external atom d_i from dpos) {
			while (enpos < enpos_end){
				ID idpos = reg->ogatoms.getIDByAddress(*enpos);
				// if (there is at least one support set in the set of kept support sets which does not have a guard, i.e. it consists only of signed input predicates of di)
					//then move to the next external atom in D1

				if (std::find(dlatnoguard.begin(), dlatnoguard.end(), idpos) != dlatnoguard.end()) {
					continue;
				}
				else {
					// go through negative external atoms
					while (enneg < enneg_end){
						ID idneg = reg->ogatoms.getIDByAddress(*enneg);
						// go through support sets for the current negative atom
						for (int i=0;i<dlatsupportsets[idneg].size();i++) {
							// check whether the current support set has a guard and
							// if it does then eliminate all support sets of atoms in dpos that contain the same guard
							BOOST_FOREACH(ID idns,dlatsupportsets[idneg][i]) {
								if (idns.isGuardAuxiliary()) {
									for (int j=0;j<dlatsupportsets[idpos].size();j++) {
										bool del=false;
										BOOST_FOREACH(ID idps,dlatsupportsets[idpos][j]) {
											if (idps==idns) {
												del=true;
											}
										}
										// delete support set which contains guard idns from the set of support sets for
										if (del==true) {
											//	dlatsupportsets[idpos].erase(j);
										}
									}
									// find out whether the guard is a concept or a role and eliminate it from the repair ABox
									// determine the type of the guard by the size of its tuple
									if (reg->ogatoms.getByAddress(idns.address).tuple.size()==2) {
										//it is a concept, thus clear the assertion from conceptABox
										newConceptsABox->clearFact(idns.address);
									}
									else {
										// it is a role, thus delete the assertion from the roleABox
									}
								}
							}
						}

					}
				}
			}

							// if (Si is empty)
				//stop with this modelCandidate, set repairExists=false 
			//else i++, i.e. move to the next external atom in D1
		//}	
		// go through all left support sets S' for exteral atoms in D2 and eliminate ABox assertions from repairABox which are involved in S'
		// return repairExists;
	DBGLOG(DBG, "***** Repair ABox existence: " << repairExists);
	return repairExists;
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
