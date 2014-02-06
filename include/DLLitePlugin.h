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
 * @file DLLitePlugin.h
 * @author Daria Stepanova
 * @author Christoph Redl
 *
 * @brief Implements interface to DL-Lite using owlcpp.
 */

#ifndef DLLITE_PLUGIN__HPP_INCLUDED_
#define DLLITE_PLUGIN__HPP_INCLUDED_

#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/PluginInterface.h"
#include "dlvhex2/ComponentGraph.h"
#include "dlvhex2/HexGrammar.h"
#include "dlvhex2/HexParserModule.h"
#include <set>

#include "owlcpp/rdf/triple_store.hpp"
#include "owlcpp/io/input.hpp"
#include "owlcpp/io/catalog.hpp"
#include "owlcpp/terms/node_tags_owl.hpp"
#include "factpp/Kernel.hpp"

DLVHEX_NAMESPACE_BEGIN

namespace dllite{

class DLPluginAtom;
class CDLAtom;
class RDLAtom;
class ConsDLAtom;
class InconsDLAtom;

class DLLitePlugin:
  public PluginInterface
{
public:
	friend class DLPluginAtom;
	friend class CDLAtom;
	friend class RDLAtom;
	friend class ConsDLAtom;
	friend class InconsDLAtom;

	// this class caches an ontology
	// add member variables here if additional information about the ontology must be stored
	struct CachedOntology{
		typedef boost::shared_ptr<ReasoningKernel> ReasoningKernelPtr;

		RegistryPtr reg;

		// meta-information from owl-file
		std::string ontologyPath, ontologyNamespace, ontologyVersion;

		ID ontologyName;			// ID of constant with filename (as given in the program or on command-line)
		bool loaded;				// true if the ontology is ready to use

		// interface to internal reasoner
		owlcpp::Triple_store store;
		ReasoningKernelPtr kernel;

		InterpretationPtr classification;	// unique model of the classification program

		// vocabulary of Tbox and Abox
		InterpretationPtr concepts, roles, individuals;

		typedef std::pair<ID, std::pair<ID, ID> > RoleAssertion;	// stores a role assertion (i1,i2) in R as <R, <i1, i2> >
		std::vector<RoleAssertion> roleAssertions;
		InterpretationPtr conceptAssertions;				// stores addresses of all true concept guard atoms

		// checks if a concept guard atom of form GuardPredID(C, I) holds
		bool checkConceptAssertion(RegistryPtr reg, ID guardAtomID) const;

		// checks if a role guard atom of form GuardPredID(R, I1, I2) holds
		bool checkRoleAssertion(RegistryPtr reg, ID guardAtomID) const;

		// returns the set of all individuals which which occur either in the Abox or in the query (including the DL-namespace)
		InterpretationPtr getAllIndividuals(const PluginAtom::Query& query);

		bool isOwlConstant(std::string str) const;
		bool containsNamespace(std::string str) const;
		std::string addNamespaceToString(std::string str) const;
		std::string removeNamespaceFromString(std::string str) const;

		CachedOntology(RegistryPtr reg);
		virtual ~CachedOntology();

		// loads the ontology
		void load(ID ontologyName);

		// computes the classification for a given ontology
		void computeClassification(ProgramCtx& ctx);

	private:
		// reads the set of concepts, roles and individuals, adds concept and role assertions
		void analyzeTboxAndAbox();
	};
	typedef boost::shared_ptr<CachedOntology> CachedOntologyPtr;

	// storage for DL-expressions in DL-syntax
	struct DLExpression{
		enum Type{ plus, minus };
		std::string conceptOrRole;
		Type type;
		ID pred;
	};

	class CtxData : public PluginData
	{
	public:
		std::vector<DLLitePlugin::CachedOntologyPtr> ontologies;
		bool repair;	// enable RepairModelGenerator?
		bool rewrite;	// automatically rewrite DL-atoms?
		bool optimize;	// automatically optimize rules with DL-atoms?
		std::string repairOntology;	// name of the ontology to repair (if repair=true)
		std::string ontology;		// name of the ontology for rewriting
		std::vector<DLExpression> dlexpressions;	// cache for DL-expressions
		CtxData() : repair(false), rewrite(false), optimize(false) {};
		virtual ~CtxData() {};
	};

private:
	RegistryPtr reg;

protected:
	// computed the DL-negation of a concept, i.e., "C" --> "-C" resp. checks if the concept is of such a form
	ID dlNeg(ID id);
	bool isDlNeg(ID id);

	// creates for concept "C" the concept "exC", removes the prefix "ex", (the same for roles) resp. checks if the concept is of such a form
	ID dlEx(ID id);
	ID dlRemoveEx(ID id);
	bool isDlEx(ID id);

	ID storeQuotedConstantTerm(std::string str);

	// check if a string starts with owl:
	bool isOwlType(std::string str) const;

	// get the part of the string after owl:
	std::string getOwlType(std::string str) const;

	// checks an owl type of form owl:str against a pattern
	bool cmpOwlType(std::string str, std::string pattern) const;

	// transforms a guard atom into a human-readable string
	std::string printGuardAtom(ID atom);

	// frequently used IDs
	ID guardPredicateID, subID, opID, confID, xID, yID, zID;

	// IDB of the classification program
	std::vector<ID> classificationIDB;

	// constructs the classification program and initialized the above frequent IDs (should be called only once)
	void constructClassificationProgram(ProgramCtx& ctx);

	// loads an ontology and computes its classification or returns a reference to it if already present
	CachedOntologyPtr prepareOntology(ProgramCtx& ctx, ID ontologyNameID);

	// creates a atom template and adds the AUX property if necessary (depending on the predicate)
	OrdinaryAtom getNewAtom(ID pred, bool ground = false);

	// creates a new guard atom template, containing only the guard predicate (further attributes must be added by the caller)
	OrdinaryAtom getNewGuardAtom(bool ground = false);

	// initializes the frequently used IDs
	void prepareIDs();
public:
	DLLitePlugin();
	virtual ~DLLitePlugin();

	virtual void processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx);

	// create parser modules that extend and the basic hex grammar
	virtual std::vector<HexParserModulePtr> createParserModules(ProgramCtx&);

	// rewrites default-negated consistency checks to inconsistency checks
	// (this makes the external atoms monotonic and may improve efficiency)
	virtual PluginRewriterPtr createRewriter(ProgramCtx&);

	virtual void printUsage(std::ostream& o) const;

	// plugin atoms
	virtual std::vector<PluginAtomPtr> createAtoms(ProgramCtx& ctx) const;

	virtual void setRegistry(RegistryPtr reg);
	virtual void setupProgramCtx(ProgramCtx& ctx);

	// RepairModelGenerator
	virtual bool providesCustomModelGeneratorFactory(ProgramCtx& ctx) const;
	virtual BaseModelGeneratorFactoryPtr getCustomModelGeneratorFactory(ProgramCtx& ctx, const ComponentGraph::ComponentInfo& ci) const;
};

}

DLVHEX_NAMESPACE_END

#endif
