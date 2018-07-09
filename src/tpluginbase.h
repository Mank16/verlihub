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

#ifndef NPLUGINTPLUGINBASE_H
#define NPLUGINTPLUGINBASE_H
#include <string>
#include <stdint.h>
#include "cobj.h"

using std::string;

namespace nVerliHub {
namespace nPlugin {
/**
the plugin base class suitable for any application

@author Daniel Muller
*/
class tPluginBase : public cObj
{
public:
	tPluginBase();
	~tPluginBase();
	bool Open();
	bool Close();
	string Error();
	bool StrLog(ostream & ostr, int level);
protected:
	string mFileName;
	string mName;
	void *mHandle;

};

	}; // namespace nPlugin
}; // namespace nVerliHub

#endif
