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
 * @file 	DLRewriter.h
 * @author 	Daria Stepanova <dasha@kr.tuwien.ac.at>
 * @author 	Christoph Redl <redl@kr.tuwien.ac.at>
 *
 * @brief Implements the rewriter from DL-syntax to HEX.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "DLLitePlugin.h"
#include "DLRewriter.h"
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

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

DLVHEX_NAMESPACE_BEGIN

namespace spirit = boost::spirit;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

// ============================== Class DLParserModuleSemantics ==============================
// (needs to be in dlvhex namespace)

namespace dllite{
extern dlvhex::dllite::DLLitePlugin theDLLitePlugin;
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

namespace dllite{

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

DLRewriter::DLRewriter(DLLitePlugin::CtxData& ctxdata) : ctxdata(ctxdata){
}

DLRewriter::~DLRewriter(){
}

void DLRewriter::rewrite(ProgramCtx& ctx){

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

void DLRewriter::addParserModule(ProgramCtx& ctx, std::vector<HexParserModulePtr>& ret, DLLitePlugin::CachedOntologyPtr ontology){
	ret.push_back(HexParserModulePtr(new DLParserModule<HexParserModule::BODYATOM>(ctx, ontology)));
}

}

DLVHEX_NAMESPACE_END

/* vim: set noet sw=2 ts=2 tw=80: */

// Local Variables:
// mode: C++
// End:
