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
#include <algorithm> 
#include "cconnchoose.h"

namespace nVerliHub {
	using namespace nEnums;
	namespace nSocket {

	cConnChoose::iterator cConnChoose::sBegin;
	cConnChoose::iterator cConnChoose::sEnd;

cConnChoose::cConnChoose()
{
	mLastSock = -1;
}


cConnChoose::~cConnChoose(){}


#if defined USE_OLD_CONNLIST || defined _WIN32
bool cConnChoose::AddConn(cConnBase *conn)
{
	if (!conn) return false;
	tSocket sock = (tSocket)(*conn);
	if (mConnList.ContainsHash(sock)) return false;
	if (sock > mLastSock) mLastSock = sock;
	bool ret = mConnList.AddWithHash(conn, sock);
	return ret;
}

bool cConnChoose::DelConn(cConnBase *conn)
{
	tSocket sock = (tSocket)(*conn);
	OptOut(conn, eCC_ALL);
 	OptOut(conn, eCC_CLOSE);
	return mConnList.RemoveByHash(sock);
}

bool cConnChoose::HasConn(cConnBase *conn)
{
	tSocket sock = (tSocket)(*conn);
	return mConnList.ContainsHash(sock);
}

inline cConnBase * cConnChoose::operator[] (tSocket sock)
{
	return mConnList.GetByHash(sock);
}

#else

bool cConnChoose::AddConn(cAsyncConn *conn)
{
	if (!conn) return false;
	//tSocket sock = (tSocket)(*conn);
	// resize
	//if ( (tSocket)mConnList.size() <= sock ) mConnList.resize(sock+sock/4, NULL);
	// don't add twice
 	//if ( ( mConnList[sock] ) > INVALID_SOCKET ) return false;
	//if (sock > mLastSock) mLastSock = sock;

	mConnList.push_back(conn);
	//mConnList[sock] = conn;
	if ( (conn->mSockDesc) > mLastSock)
			mLastSock = conn->mSockDesc;
	return true;
}

bool cConnChoose::DelConn(cAsyncConn *conn)
{
	//tSocket sock = (tSocket)(*conn);
	//if ( (tSocket)mConnList.size() <= sock ) return false;
	OptOut(conn, eCC_ALL);
 	OptOut(conn, eCC_CLOSE);
	//mConnList[sock] = NULL;
	vector<cAsyncConn*>::const_iterator cver;
	for (vector<cAsyncConn*>::const_iterator it = mConnList.cbegin();it!=mConnList.cend();++it)
	{
				if( (*it)->mSockDesc == conn->mSockDesc) // find
				{
					cver = it;
					break;
				}								
	}
	mConnList.erase(cver);
	return true;
}

bool cConnChoose::HasConn(cAsyncConn *conn)
{
//	tSocket sock = (tSocket)(*conn);
//	if ( (tSocket)mConnList.size() <= sock ) return false;
//	return mConnList[sock] != NULL;
    vector<cAsyncConn*>::iterator results = std::find(std::begin(mConnList), std::end(mConnList), conn);
	return results != std::end(mConnList);
}

inline cConnBase * cConnChoose::operator[] (tSocket sock)
{
	/*if(tSocket(mConnList.size()) > sock)
		return mConnList[sock];
	else
		return NULL;*/

	for (vector<cAsyncConn*>::iterator it = mConnList.begin();it!=mConnList.end();++it)
	{
				if( (*it)->mSockDesc == sock)
	                      return *it;
	}	
		
}
#endif

inline void cConnChoose::OptIn(cConnBase* conn, nEnums::tChEvent mask)
{
	if(!conn)
		return;
	this->OptIn(tSocket(*conn), mask);
}

inline void cConnChoose::OptOut(cConnBase* conn, nEnums::tChEvent mask)
{
	if(!conn)
		return;
	this->OptOut(tSocket(*conn), mask);
}

inline int cConnChoose::OptGet(cConnBase *conn)
{
	if(!conn)
		return 0;
	return this->OptGet(tSocket(*conn));
}

inline int cConnChoose::RevGet(cConnBase *conn)
{
	if(!conn)
		return 0;
	return this->RevGet(tSocket(*conn));
}

inline bool cConnChoose::RevTest(cConnBase *conn)
{
	if(!conn)
		return false;
	return this->RevTest(tSocket(*conn));
}

inline tSocket cConnChoose::operator[] (cConnBase *conn)
{
	if(!conn)
		return INVALID_SOCKET;
	return (tSocket)(*conn);
}
	}; // namespace nSocket
}; // namespace nVerliHub
