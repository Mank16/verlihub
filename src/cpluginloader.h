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

#ifndef NPLUGINCPLUGINLOADER_H
#define NPLUGINCPLUGINLOADER_H
#include <string>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <dlfcn.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "cpluginbase.h"

using std::string;
namespace nVerliHub {
	namespace nPlugin {
/**
class for encapsulating plugins to be loaded

@author Daniel Muller
*/
class cPluginLoader : public cObj
{
public:
	cPluginLoader(const string &filename);
	~cPluginLoader();
 	bool Open();
	bool Close();
	bool LoadSym();
	bool StrLog(ostream & ostr, int level);
	bool IsError()
	{
		return (mError = dlerror()) != NULL;
	}
	string Error(){return string((mError!=NULL)?mError:"ok");}
	string GetFilename(){ return mFileName;}
	void * LoadSym(const char *);
	cPluginBase *mPlugin;

protected:
	string mFileName;
	const char * mError;
	#ifdef _WIN32
	HINSTANCE mHandle;
	#else
	void *mHandle;
	#endif
	typedef cPluginBase *(*tcbGetPluginFunc)(void);
	typedef void (*tcbDelPluginFunc)(cPluginBase *);
	tcbDelPluginFunc mcbDelPluginFunc;
	tcbGetPluginFunc mcbGetPluginFunc;
};

	}; // namespace nPlugin
};  // namespace nVerliHub

#endif
