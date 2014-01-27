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
#include <set>

#include "owlcpp/rdf/triple_store.hpp"
#include "owlcpp/io/input.hpp"
#include "owlcpp/io/catalog.hpp"
#include "owlcpp/terms/node_tags_owl.hpp"
#include "factpp/Kernel.hpp"

DLVHEX_NAMESPACE_BEGIN

namespace dllite{

class DLLitePlugin:
  public PluginInterface
{
public:
	// this class caches an ontology
	// add member variables here if additional information about the ontology must be stored
	struct CachedOntology{
		typedef boost::shared_ptr<ReasoningKernel> ReasoningKernelPtr;

		RegistryPtr reg;

		ID ontologyName;
		std::string ontologyPath, ontologyNamespace, ontologyVersion;
		bool loaded;
		owlcpp::Triple_store store;
		InterpretationPtr classification;
		ReasoningKernelPtr kernel;

		InterpretationPtr concepts, roles, individuals;

		typedef std::pair<ID, std::pair<ID, ID> > RoleAssertion;
		InterpretationPtr conceptAssertions;
		std::vector<RoleAssertion> roleAssertions;
		inline bool checkConceptAssertion(RegistryPtr reg, ID guardAtomID) const;
		inline bool checkRoleAssertion(RegistryPtr reg, ID guardAtomID) const;

		// returns the set of all individuals which which occur either in the Abox or in the query (including the DL-namespace)
		InterpretationPtr getAllIndividuals(const PluginAtom::Query& query);

		inline bool containsNamespace(std::string str) const;
		inline bool containsNamespace(ID term) const;
		inline bool isOwlType(std::string str) const;
		inline std::string addNamespaceToString(std::string str) const;
		inline std::string removeNamespaceFromString(std::string str) const;
		inline ID addNamespaceToTerm(ID term);
		inline ID removeNamespaceFromTerm(ID term);
		inline ID addNamespaceToAtom(ID atom);
		inline ID removeNamespaceFromAtom(ID atom);
		inline InterpretationPtr addNamespaceToInterpretation(InterpretationPtr intr);
		inline InterpretationPtr removeNamespaceFromInterpretation(InterpretationPtr intr);

		CachedOntology(RegistryPtr reg);
		virtual ~CachedOntology();

		void load(RegistryPtr reg, ID ontologyName);
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
		std::string repairOntology;	// name of the ontology to repair (if repair=true)
		std::string ontology;		// name of the ontology for rewriting
		std::vector<DLExpression> dlexpressions;	// cache for DL-expressions
		CtxData() : repair(false), rewrite(false) {};
		virtual ~CtxData() {};
	};

	// base class for all DL atoms
	class DLPluginAtom : public PluginAtom{
	private:
		bool learnedSupportSets;
	protected:
		ProgramCtx& ctx;

		// IDB of the classification program
		std::vector<ID> classificationIDB;

		// special predicate for guard atoms
		ID guardPredicate;

		// computed the DL-negation of a concept, i.e., "C" --> "-C" resp. checks if the concept is of such a form
		inline ID dlNeg(ID id);
		inline bool isDlNeg(ID id);

		// creates for concept "C" the concept "exC" (the same for roles) resp. checks if the concept is of such a form
		inline ID dlEx(ID id);
		inline bool isDlEx(ID id);

		// extracts from a string the postfix after the given symbol
		static inline std::string afterSymbol(std::string str, char c = '#');

		// transforms a guard atom into a human-readable string
		inline std::string printGuardAtom(ID atom);

		// frequently used IDs
		ID subID, opID, confID, xID, yID, zID;

		// constructs the classification program and initialized the above frequent IDs (should be called only once)
		void constructClassificationProgram();

		// computes the classification for a given ontology
		InterpretationPtr computeClassification(ProgramCtx& ctx, CachedOntologyPtr ontology);

		// constructs the concept and role assertions
		static void constructAbox(ProgramCtx& ctx, CachedOntologyPtr ontology);

		// loads an ontology and computes its classification or returns a reference to it if already present
		CachedOntologyPtr prepareOntology(ProgramCtx& ctx, ID ontologyNameID);

		// checks the guard atoms wrt. the Abox, removes them from ng and sets keep to true in this case, and sets keep to false otherwise
		virtual void guardSupportSet(bool& keep, Nogood& ng, const ID eaReplacement);

		// learns a complete set of support sets for the ontology specified in query.input[0] and adds them to nogoods
		void learnSupportSets(const Query& query, NogoodContainerPtr nogoods);

		// expands the Abox with the facts given in the interpretation
		std::vector<TDLAxiom*> expandAbox(const Query& query);

		// recorvers the original Abox
		void restoreAbox(const Query& query, std::vector<TDLAxiom*> addedAxioms);

		// used for query answering using FaCT++
		class Actor_collector{
		public:
			enum Type{Concept, Role};
		private:
			RegistryPtr reg;
			Type type;
			Tuple currentTuple;
			Answer& answer;
			CachedOntologyPtr ontology;
		public:
			Actor_collector(RegistryPtr reg, Answer& answer, CachedOntologyPtr ontology, Type t);
			virtual ~Actor_collector();
			bool apply(const TaxonomyVertex& node);
			void processTuple(Tuple tup);
		};

	public:
		// loads an ontology and retrives concepts and roles, but does not compute its classification
		static CachedOntologyPtr initializeOntology(ProgramCtx& ctx, ID ontologyNameID);

		DLPluginAtom(std::string predName, ProgramCtx& ctx);

		virtual void retrieve(const Query& query, Answer& answer);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
	};

	// concept queries
	class CDLAtom : public DLPluginAtom{
	public:
		CDLAtom(ProgramCtx& ctx);
		virtual void retrieve(const Query& query, Answer& answer);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
	};

	// role queries
	class RDLAtom : public DLPluginAtom{
	public:
		RDLAtom(ProgramCtx& ctx);
		virtual void retrieve(const Query& query, Answer& answer);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
	};

	// consistency check
	class ConsDLAtom : public DLPluginAtom{
	public:
		ConsDLAtom(ProgramCtx& ctx);
		virtual void retrieve(const Query& query, Answer& answer);
		virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
	};

	DLLitePlugin();
	virtual ~DLLitePlugin();

	virtual void processOptions(std::list<const char*>& pluginOptions, ProgramCtx& ctx);

	// create parser modules that extend and the basic hex grammar
	virtual std::vector<HexParserModulePtr> createParserModules(ProgramCtx&);

	virtual void printUsage(std::ostream& o) const;

	// plugin atoms
	virtual std::vector<PluginAtomPtr> createAtoms(ProgramCtx& ctx) const;

	// RepairModelGenerator
	virtual bool providesCustomModelGeneratorFactory(ProgramCtx& ctx) const;
	virtual BaseModelGeneratorFactoryPtr getCustomModelGeneratorFactory(ProgramCtx& ctx, const ComponentGraph::ComponentInfo& ci) const;
};

}

DLVHEX_NAMESPACE_END

#endif
