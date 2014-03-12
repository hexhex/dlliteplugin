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
 * @file 	DLLitePlugin.h
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
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
#include "dlvhex2/Printer.h"
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
	friend class RepairModelGenerator;

	// this class caches an ontology
	// add member variables here if additional information about the ontology must be stored
	struct CachedOntology{
		typedef boost::shared_ptr<ReasoningKernel> ReasoningKernelPtr;

		RegistryPtr reg;

		// meta-information from owl-file
		std::string ontologyPath, ontologyNamespace, ontologyVersion;

		ID ontologyName;			// ID of constant with filename (as given in the program or on command-line)
		bool includeAbox;			// true if the ontology was loaded including the Abox, false if it was loaded with empty Abox
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
		//InterpretationPtr AboxPredicates;
		std::vector<ID> AboxPredicates;

		// checks if a concept guard atom of form GuardPredID(C, I) holds
		bool checkConceptAssertion(RegistryPtr reg, ID guardAtomID) const;

		// checks if a role guard atom of form GuardPredID(R, I1, I2) holds
		bool checkRoleAssertion(RegistryPtr reg, ID guardAtomID) const;

		// returns the set of all individuals which which occur either in the Abox or in the query (including the DL-namespace)
		InterpretationPtr getAllIndividuals(const PluginAtom::Query& query);

		bool isOwlConstant(std::string str) const;
		
		inline bool containsNamespace(std::string str) const{
			return (str.substr(0, ontologyNamespace.length()) == ontologyNamespace || str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace);
		}
		
		inline std::string addNamespaceToString(std::string str) const{
			if (str[0] == '-') return "-" + ontologyNamespace + "#" + str.substr(1);
			else return ontologyNamespace + "#" + str;
		}
		
		inline std::string removeNamespaceFromString(std::string str) const{
			if (!(str.substr(0, ontologyNamespace.length()) == ontologyNamespace || (str[0] == '-' && str.substr(1, ontologyNamespace.length()) == ontologyNamespace))){
				DBGLOG(WARNING, "Constant \"" + str + "\" appears to be a constant of the ontology, but does not contain its namespace.");
				return str;
			}
			if (str[0] == '-') return '-' + str.substr(ontologyNamespace.length() + 1 + 1); // +1 because of '-', +1 because of '#'
			return str.substr(ontologyNamespace.length() + 1); // +1 because of '#'
		}


		CachedOntology(RegistryPtr reg);
		virtual ~CachedOntology();

		// loads the ontology
		void load(ID ontologyName, bool includeAbox);

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
	inline ID dlNeg(ID id){
		if (reg->terms.getByID(id).getUnquotedString()[0] == '-') return storeQuotedConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(1));
		else return storeQuotedConstantTerm("-" + reg->terms.getByID(id).getUnquotedString());
	}
	
	inline bool isDlNeg(ID id){
		return (reg->terms.getByID(id).getUnquotedString()[0] == '-');
	}
	
	// creates for role "R" the concept "exR", removes the prefix "ex", if the concept is of such a form
	inline ID dlEx(ID id){
		return storeQuotedConstantTerm("Ex:" + reg->terms.getByID(id).getUnquotedString());
	}
	
	inline ID dlRemoveEx(ID id){
		assert(isDlEx(id) && "tried to translate exC to C, but given term is not of form exC");
		return storeQuotedConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(3));
	}
	
	inline bool isDlInv(ID id){
		return (reg->terms.getByID(id).getUnquotedString().substr(0, 4).compare("Inv:") == 0);
	}
	// creates for concept "C" the concept "exC", removes the prefix "ex", (the same for roles) resp. checks if the concept is of such a form
	inline ID dlInv(ID id){
		return storeQuotedConstantTerm("Inv:" + reg->terms.getByID(id).getUnquotedString());
	}

	inline ID dlRemoveInv(ID id){
		assert(isDlInv(id) && "tried to translate invC to C, but given term is not of form invC");
		return storeQuotedConstantTerm(reg->terms.getByID(id).getUnquotedString().substr(4));
	}


	inline ID storeQuotedConstantTerm(std::string str){
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
	
	// check if a string starts with owl:
	inline bool isOwlType(std::string str) const{
	
		// add prefixes to recognize here
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		if (str.length() > 4 && str.substr(0, 4).compare("owl:") == 0) return true;
		if (str.length() > 4 && str.substr(0, 4).compare("rdf:") == 0) return true;
		if (str.length() > 5 && str.substr(0, 5).compare("rdfs:") == 0) return true;
		return false;
	}
	
	// get the part of the string after owl:
	inline std::string getOwlType(std::string str) const{
	
	        if (str.find_last_of(':') == std::string::npos) return str;
	        else return str.substr(str.find_last_of(':') + 1);
	
	//	assert(isOwlType(str) && "tried to get the type of a string which does not start with owl:");
	//	return str.substr(4);
	}
	
	// checks an owl type of form owl:str against a pattern
	inline bool cmpOwlType(std::string str, std::string pattern) const{
	
		if (!isOwlType(str)) return false;
		std::string extracted = getOwlType(str);
	
		std::transform(extracted.begin(), extracted.end(), extracted.begin(), ::tolower);
		std::transform(pattern.begin(), pattern.end(), pattern.begin(), ::tolower);
	
		return extracted == pattern;
	}
	
	inline bool isDlEx(ID id){
		return (reg->terms.getByID(id).getUnquotedString().substr(0, 3).compare("Ex:") == 0);
	}
	
	// transforms a guard atom into a human-readable string
	inline std::string printGuardAtom(ID atom){
	
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

	// frequently used IDs
	ID guardPredicateID, subID, opID, confID, xID, yID, zID;

	// IDB of the classification program
	std::vector<ID> classificationIDB;

	// constructs the classification program and initialized the above frequent IDs (should be called only once)
	void constructClassificationProgram(ProgramCtx& ctx);

	// loads an ontology and computes its classification or returns a reference to it if already present
	CachedOntologyPtr prepareOntology(ProgramCtx& ctx, ID ontologyNameID, bool includeAbox = true);

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
