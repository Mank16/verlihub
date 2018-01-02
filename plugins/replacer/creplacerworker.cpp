/*
	Copyright (C) 2003-2005 Daniel Muller, dan at verliba dot cz
	Copyright (C) 2006-2017 Verlihub Team, info at verlihub dot net

	Verlihub is free software; You can redistribute it
	and modify it under the terms of the GNU General
	Public License as published by the Free Software
	Foundation, either version 3 of the license, or at
	your option any later version.

	Verlihub is distributed in the hope that it will be
	useful, but without any warranty, without even the
	implied warranty of merchantability or fitness for
	a particular purpose. See the GNU General Public
	License for more details.

	Please see http://www.gnu.org/licenses/ for a copy
	of the GNU General Public License.
*/

#include "creplacerworker.h"

namespace nVerliHub {
	namespace nReplacePlugin {
cReplacerWorker::cReplacerWorker()
{
	mAfClass = 4; // default affected class is <= 4 (operator)
}

cReplacerWorker::~cReplacerWorker()
{
}

bool cReplacerWorker::CheckMsg(const string &msg)
{
	return (0 < mRegex.Exec(msg));
}

bool cReplacerWorker::PrepareRegex()
{
	return mRegex.Compile(mWord.data(),PCRE_CASELESS);
}

	}; // namespace nReplacePlugin
}; // namespace nVerliHub
