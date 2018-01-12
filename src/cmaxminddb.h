/*
	Copyright (C) 2003-2005 Daniel Muller, dan at verliba dot cz
	Copyright (C) 2006-2018 Verlihub Team, info at verlihub dot net

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

#ifndef NUTILSCMAXMINDDB_H
#define NUTILSCMAXMINDDB_H

#include "cobj.h"
#include <string>
#include <ostream>
#include <maxminddb.h>
#include <unicode/translit.h>

using std::string;

namespace nVerliHub {
	namespace nSocket {
		class cServerDC;
	};

	namespace nUtils {

		class cMaxMindDB: public cObj
		{
			public:
				cMaxMindDB(nSocket::cServerDC *mS);
				~cMaxMindDB();

				void ReloadAll();
				void ShowInfo(ostream &os);

				bool GetCC(const string &host, string &cc);
				bool GetCN(const string &host, string &cn);
				bool GetCity(string &geo_city, const string &host, const string &db = "");
				bool GetCCC(string &geo_cc, string &geo_cn, string &geo_ci, const string &host, const string &db = ""); // optimized function used on user login, gets all 3 values from 1 lookup
				bool GetGeoIP(string &geo_host, string &geo_ran_lo, string &geo_ran_hi, string &geo_cc, string &geo_ccc, string &geo_cn, string &geo_reg_code, string &geo_reg_name, string &geo_tz, string &geo_cont, string &geo_city, string &geo_post, double &geo_lat, double &geo_lon, unsigned short &geo_met, unsigned short &geo_area, const string &host, const string &db = "");
				bool GetASN(string &asn_name, const string &host, const string &db = "");
			private:
				nSocket::cServerDC *mServ;
				Transliterator *mTran;

				MMDB_s *mDBCO;
				MMDB_s *mDBCI;
				MMDB_s *mDBAS;

				MMDB_s *TryCountryDB(unsigned int flags);
				MMDB_s *TryCityDB(unsigned int flags);
				MMDB_s *TryASNDB(unsigned int flags);

				const string &FromUTF8(const string &data, string &back, const string &tset/*, const string &fset = "UTF-8"*/);
				const string &TranUTF8(const string &data, string &back);
				const string &WorkUTF8(const char *udat, unsigned int ulen, string &back, const string &tset);

				bool FileExists(const char *name);
				unsigned long FileSize(const char *name);
		};

	};
};

#endif
