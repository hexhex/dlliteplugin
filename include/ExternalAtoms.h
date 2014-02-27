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
 * @file 	ExternalAtoms.h
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
 *
 * @brief Implements the external atoms of the dllite plugin.
 */

#ifndef EXTERNALATOMS__HPP_INCLUDED_
#define EXTERNALATOMS__HPP_INCLUDED_

#include "DLLitePlugin.h"
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

// base class for all DL atoms
class DLPluginAtom : public PluginAtom{
protected:
	ProgramCtx& ctx;
	RegistryPtr reg;

	// checks the guard atoms wrt. the Abox, removes them from ng and sets keep to true in this case, and sets keep to false otherwise
	virtual void guardSupportSet(bool& keep, Nogood& ng, const ID eaReplacement);

	// expands the Abox with the facts given in the interpretation
	std::vector<TDLAxiom*> expandAbox(const Query& query, bool useExistingAbox);

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
		DLLitePlugin::CachedOntologyPtr ontology;
	public:
		Actor_collector(RegistryPtr reg, Answer& answer, DLLitePlugin::CachedOntologyPtr ontology, Type t);
		virtual ~Actor_collector();
		bool apply(const TaxonomyVertex& node);
		void processTuple(Tuple tup);
	};
	bool changeABox(const Query& query);
public:
	DLPluginAtom(std::string predName, ProgramCtx& ctx, bool monotonic = true);
	virtual void retrieve(const Query& query, Answer& answer);
	virtual void learnSupportSets(const Query& query, NogoodContainerPtr nogoods);
	void optimizeSupportSets(SimpleNogoodContainerPtr initial, NogoodContainerPtr final);
};

// concept queries
class CDLAtom : public DLPluginAtom{
public:
	CDLAtom(ProgramCtx& ctx);
	virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
};

// role queries
class RDLAtom : public DLPluginAtom{
public:
	RDLAtom(ProgramCtx& ctx);
	virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
};

// consistency check
class ConsDLAtom : public DLPluginAtom{
public:
	ConsDLAtom(ProgramCtx& ctx);
	virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
};

// inconsistency check
class InconsDLAtom : public DLPluginAtom{
public:
	InconsDLAtom(ProgramCtx& ctx);
	virtual void retrieve(const Query& query, Answer& answer, NogoodContainerPtr nogoods);
};

}

DLVHEX_NAMESPACE_END

#endif
