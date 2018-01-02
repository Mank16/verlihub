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

#include "ccallbacklist.h"
#include "cpluginbase.h"
#include "cpluginmanager.h"

namespace nVerliHub {
	namespace nPlugin {

cCallBackList::cCallBackList(cPluginManager *mgr, string id) :
mMgr(mgr),
mCallOne(mMgr,this),
mName(id)
{
	if(mMgr)
		mMgr->SetCallBack(id, this);
}

const string &cCallBackList::Name() const
{
	return mName;
}

cCallBackList::~cCallBackList()
{}

void cCallBackList::ufCallOne::operator()(cPluginBase *pi)
{
	if(mCall)
		mCall = mCBL->CallOne(pi);
	// If the plugin is not alive, unload it with plugin manager
	if(!pi->IsAlive())
		mMgr->UnloadPlugin(pi->Name());
}

bool cCallBackList::Register(cPluginBase *plugin)
{
	if(!plugin)
		return false;
	tPICont::iterator i = find(mPlugList.begin(), mPlugList.end(), plugin);
	if(i != mPlugList.end())
		return false;
	mPlugList.push_back(plugin);
	return true;
}

bool cCallBackList::Unregister(cPluginBase *plugin)
{
	if(!plugin)
		return false;
	tPICont::iterator i = find(mPlugList.begin(), mPlugList.end(), plugin);
	if(i == mPlugList.end())
		return false;
	mPlugList.erase(i);
	return true;
}

bool cCallBackList::CallAll()
{
	mCallOne.mCall = true;
	return for_each(mPlugList.begin() , mPlugList.end(), mCallOne).mCall;
}

void cCallBackList::ListRegs(ostream &os, const string &sep)
{
	for (tPICont::iterator i = mPlugList.begin(); i != mPlugList.end(); ++i)
		os << (*i)->Name() << sep;
}

	}; // namespace nPlugin
}; // namespace nVerliHub
