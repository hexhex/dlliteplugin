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
 * @file DLRewriter.h
 * @author Daria Stepanova
 * @author Christoph Redl
 *
 * @brief Implements the rewriter from DL-syntax to HEX.
 */

#ifndef DLREWRITER__HPP_INCLUDED_
#define DLREWRITER__HPP_INCLUDED_

#include "DLLitePlugin.h"

#include "dlvhex2/PlatformDefinitions.h"
#include "dlvhex2/PluginInterface.h"
#include "dlvhex2/ComponentGraph.h"
#include "dlvhex2/HexGrammar.h"
#include "dlvhex2/HexParserModule.h"

DLVHEX_NAMESPACE_BEGIN

namespace dllite{

class DLRewriter:
	public PluginRewriter
{
private:
	DLLitePlugin::CtxData& ctxdata;

public:
	DLRewriter(DLLitePlugin::CtxData& ctxdata);
	virtual ~DLRewriter();

	virtual void rewrite(ProgramCtx& ctx);
	static void addParserModule(ProgramCtx& ctx, std::vector<HexParserModulePtr>& ret, DLLitePlugin::CachedOntologyPtr ontology);
};

}

DLVHEX_NAMESPACE_END

#endif
