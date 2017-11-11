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

#include "casyncsocketserver.h"
//#if defined _WIN32
//#  include <Winsock2.h>
//#else
#include <sys/socket.h>
//#endif
#include <unistd.h>
#include <stdio.h>
#include <algorithm>

using namespace std;

namespace nVerliHub {
	using namespace nUtils;
	using namespace nEnums;
	using namespace nSocket;
	namespace nSocket {

	bool nSocket::cAsyncSocketServer::WSinitialized = false;

cAsyncSocketServer::cAsyncSocketServer(int port):
	cObj("cAsyncSocketServer"),
	mAddr("0.0.0.0"),
	mAddr6("::"),
	mPort(port),
	timer_conn_period(4),
	timer_serv_period(1),
	mStepDelay(0),
	mMaxLineLength(10240),
	mUseDNS(0),
	mFrequency(mTime, 90.0, 20),
	mbRun(false),
	mFactory(NULL),
	mRunResult(0),
	mNowTreating(NULL)
{
	/*#ifdef _WIN32
	if(!this->WSinitialized) {

		WORD wVersionRequested;
		WSADATA wsaData;
		int err;

		wVersionRequested = MAKEWORD(2, 2);

		err = WSAStartup(wVersionRequested, &wsaData);
		if(err != 0) {
			/* Tell the user that we could not find a usable */
			/* WinSock DLL.                                  */
	//		return;
	//	}

		/*
		 * Confirm that the WinSock DLL supports 2.2.
		 * Note that if the DLL supports versions greater
		 * than 2.2 in addition to 2.2, it will still return
		 * 2.2 in wVersion since that is the version we
		 * requested.
		 */
	//	if(LOBYTE( wsaData.wVersion ) != 2 ||  HIBYTE( wsaData.wVersion ) != 2) {
			/* Tell the user that we could not find a usable
			 * WinSock DLL.
			 */
	//		WSACleanup();
	//		return;
	//	}

		// The WinSock DLL is acceptable. Proceed.
	//	this->WSinitialized = true;
	//}
	//#endif
}

cAsyncSocketServer::~cAsyncSocketServer()
{
	close();
	/*#ifdef _WIN32
	WSACleanup();
	#endif*/
	vhLog(2) << "Allocated objects: " << cObj::GetCount() << endl;
	vhLog(2) << "Unclosed sockets: " << cAsyncConn::sSocketCounter << endl;
}

int cAsyncSocketServer::run()
{
	mT.stop = cTime(0, 0);
	mbRun = true;
	cTime now;

	vhLog(1) << "Main loop start" << endl;

	while (mbRun) {
		mTime.Get();
		TimeStep();

		if (mTime >= (mT.main + timer_serv_period)) {
			mT.main = mTime;
			OnTimerBase(mTime);
		}

		//#if !defined _WIN32
			::usleep(mStepDelay * 1000);
		//#else
		//	::Sleep(mStepDelay);
		//#endif

		mFrequency.Insert(mTime);

		if (mT.stop.Sec() && (mTime >= mT.stop))
			mbRun = false;
	}

	vhLog(1) << "Main loop stop with code " << mRunResult << endl;

	return mRunResult;
}

void cAsyncSocketServer::stop(int code, int delay)
{
	if (delay == -1)
		mT.stop = cTime(0, 0);
	else
		mT.stop = (mTime + (int)delay);

	mRunResult = code;
}

void cAsyncSocketServer::close()
{
	mbRun = false;
	tCLIt it;

	for (it = mConnList.begin(); it != mConnList.end(); ++it) {
		if (*it) {
			mConnChooser.DelConn(*it);

			if (mFactory) {
				mFactory->DeleteConn(*it);
			} else {
				delete (*it);
				(*it) = NULL;
			}
		}
	}
}

unsigned int cAsyncSocketServer::getPort() const
{
	return mPort;
}

void cAsyncSocketServer::setPort(unsigned int _newVal)
{
	mPort = _newVal;
}

void cAsyncSocketServer::addConnection(cAsyncConn *new_conn)
{

	if(!new_conn)
		throw "addConnection null pointer";
	
	/*if(new_conn->mSockDesc == INVALID_SOCKET) {//socket was not created?
		if(new_conn->Log(3))
			new_conn->LogStream() << "Access refused " << new_conn->AddrIP() << endl;
		new_conn->mxMyFactory->DeleteConn(new_conn);
		return;
	}*/

	mConnChooser.AddConn(new_conn);

	mConnChooser.cConnChoose::OptIn((cConnBase *)new_conn, tChEvent(eCC_INPUT | eCC_ERROR));
	tCLIt it = mConnList.insert(mConnList.begin(), new_conn);

	new_conn->mIterator = it;
	if(0 > OnNewConn(new_conn))
		delConnection(new_conn);
}

void cAsyncSocketServer::delConnection(cAsyncConn *old_conn)
{
	if (!old_conn)
		throw "delConnection null pointer";

	if (mNowTreating == old_conn) {
		//old_conn->ok = false;
		delete old_conn;
		return;
	}

	bool badit = false;
	tCLIt emptyit;
	tCLIt it = old_conn->mIterator;

	/*
		check iterator before creating a pointer to it if we dont want a segmentation fault
		this sometimes happens with larger amount of users
		todo: does this leave any memory leaks?
	*/
	if ((it == mConnList.end()) || (it == emptyit)) {
		vhErr(1) << "Invalid iterator for connection: " << old_conn << endl;
		badit = true;
		//throw "Deleting connection without iterator";
	}

	if (!badit) {
		cAsyncConn *found = (*it);

		if (found != old_conn) {
			vhErr(1) << "Connection not found: " << old_conn << endl;
			throw "Deleting non existing connection";
		}
	}

	mConnChooser.DelConn(old_conn);

	if (!badit)
		mConnList.erase(it);

	old_conn->mIterator = emptyit;

	if (old_conn->mxMyFactory)
		old_conn->mxMyFactory->DeleteConn(old_conn);
	else
		delete old_conn;
}

int cAsyncSocketServer::input(cAsyncConn *conn)
{
	int just_read = 0;
	// Read all data available into a buffer
	if(conn->ReadAll() <= 0)
		return 0;
	while( (conn->getok()) && conn->mWritable) {
		// Create new line obj if necessary
		if(conn->LineStatus() == AC_LS_NO_LINE)
			conn->SetLineToRead(FactoryString(conn),'|',mMaxLineLength);
		// Read data into it from the buffer
		just_read += conn->ReadLineLocal();
		if(conn->LineStatus() == AC_LS_LINE_DONE) {
			OnNewMessage(conn,conn->GetLine());
			conn->ClearLine();
			// Connection may be closed after this
		}
		if(conn->BufferEmpty())
			break;
	}
	return just_read;
}

int cAsyncSocketServer::output(cAsyncConn * conn)
{
	conn->Flush();
	return 0;
}

void cAsyncSocketServer::OnNewMessage(cAsyncConn *, string *str)
{
	delete str;
}

string * cAsyncSocketServer::FactoryString(cAsyncConn *)
{
	return new string();
}

void cAsyncSocketServer::OnConnClose(cAsyncConn* conn)
{
	if(!conn)
		return;
	mConnChooser.DelConn(conn);
}

int cAsyncSocketServer::OnNewConn(cAsyncConn*conn)
{
	if(!conn)
		return -1;
	return 0;
}

int cAsyncSocketServer::OnTimerBase(cTime &now)
{
	tCLIt it;
	OnTimer(now);

	if ((mT.conn + timer_conn_period) <= now) {
		mT.conn = now;

		for (it=mConnList.begin(); it != mConnList.end(); ++it) {
			if ((*it)->mSockDesc > INVALID_SOCKET)
				(*it)->OnTimerBase(now);
		}
	}

	return 0;
}

int cAsyncSocketServer::OnTimer(cTime &now)
{
	return 0;
}

void cAsyncSocketServer::TimeStep()
{
	cTime tmout(0,1000l);
	{
		int n = mConnChooser.Choose(tmout);
		if(!n) {
			//#if ! defined _WIN32
			::usleep(50);
			//#else
			//::Sleep(0);
			//#endif
			return;
		}
	}

#if !USE_SELECT
	cConnChoose::iterator it;
#else
	cConnSelect::iterator it;
#endif
	cConnChoose::sChooseRes res;
	for(it = mConnChooser.begin(); it != mConnChooser.end(); ) {
		res = (*it);
		++it;
		mNowTreating = (cAsyncConn* )res.mConn;
		cAsyncConn *conn = mNowTreating;
		int activity = res.mRevent;
		bool bOK = false;
		if(conn)
		   bOK = conn->getok();

		if(!mNowTreating)
			continue;
		// Some connections may have been disabled during this loop so skip them
		if(bOK && ( (activity & eCC_INPUT) == eCC_INPUT ) && conn->GetType() == eCT_LISTEN) {
			// Accept incoming connection
			int i = 0;
			cAsyncConn *new_conn = NULL;
			do {

				new_conn = conn->Accept();

				if(new_conn) addConnection(new_conn);
				i++;
			} while(new_conn && i <= 101);
//#ifdef _WIN32
//			vhLog(1) << "num connections" << mConnChooser.mConnList.size() << endl;
//#endif

		}
		if(bOK && ( (activity & eCC_INPUT) == eCC_INPUT ) &&
			((conn->GetType() == eCT_CLIENT) || (conn->GetType() == eCT_CLIENTUDP)))
			// Data to be read or data in buffer
		{
			if(input(conn) <= 0)
				bOK = false;
		}
		if(bOK && ((activity & eCC_OUTPUT) == eCC_OUTPUT)) {
			// NOTE: in sockbuf::write is a bug, missing buf increment, it will block until whole buffer is sent
			output(conn);
		}
		mNowTreating = NULL;
		if(!bOK || ( (activity & (eCC_ERROR | eCC_CLOSE))==(eCC_ERROR | eCC_CLOSE) )) {

			delConnection(conn);
		}
	}
}

pair<cAsyncConn *,cAsyncConn*> cAsyncSocketServer::Listen(int OnPort, bool bUDP)
{
	cAsyncConn *ListenSock;
	cAsyncConn* ListenSock6;

	if(!bUDP) {
		ListenSock = new cAsyncConn(0, this, eCT_LISTEN);
		ListenSock6 = new cAsyncConn(0, this, eCT_LISTEN, true);
		
	} else {
		ListenSock = new cAsyncConn(0, this, eCT_CLIENTUDP);
		ListenSock6 = new cAsyncConn(0, this, eCT_CLIENTUDP, true);
	}
	
	if(this->ListenWithConn(ListenSock, OnPort, bUDP) == NULL )
		LogStream() << "Error with IPv4";
		
	if(this->ListenWithConn(ListenSock6, OnPort, bUDP,true) == NULL)
		LogStream() << "Error with IPv6";
	
	return make_pair(ListenSock,ListenSock6);
}


int cAsyncSocketServer::StartListening(int OverrideDefaultPort)
{
	if(OverrideDefaultPort && !mPort)
		mPort = OverrideDefaultPort;
	if(mPort && !OverrideDefaultPort)
		OverrideDefaultPort = mPort;
	// ipv4/ipv6
	pair<cAsyncConn*, cAsyncConn*> mIsOk = this->Listen(OverrideDefaultPort, false);
	
	if(mIsOk.first != NULL && mIsOk.second != NULL)
		return 0;
	return -1;
}

void cAsyncSocketServer::LogOnListen(std::string mess, int OnPort, bool bipv6 , bool bUDP)
{
	if(Log(0))
	{
		LogStream() << mess << (bipv6 ? "["+mAddr6+"]:" : mAddr) << OnPort << (bUDP ? " UDP":" TCP" ) << endl;
	}
	
}	

cAsyncConn * cAsyncSocketServer::ListenWithConn(cAsyncConn *ListenSock, int OnPort, bool bUDP, bool bipv6)
{
	if(ListenSock != NULL) {
		if(ListenSock->ListenOnPort(OnPort, bipv6 ? (const_cast<char*>(mAddr6.c_str())) : (const_cast<char*>(mAddr.c_str())), bUDP,bipv6) == INVALID_SOCKET) {
			LogOnListen("Can not listen on " , OnPort, bipv6 ,bUDP );
			LogStream() << "Please make sure the port is open and not already used by another process" << endl;
		}
		this->mConnChooser.AddConn(ListenSock);
		this->mConnChooser.cConnChoose::OptIn(
			(cConnBase *)ListenSock,
			tChEvent(eCC_INPUT|eCC_ERROR));
		
		LogOnListen("Listen for Connection on " , OnPort ,bipv6 ,bUDP );	
		return ListenSock;
	}
	return NULL;
}

bool cAsyncSocketServer::StopListenConn(cAsyncConn *connection)
{
	if (connection != NULL) {
		this->mConnChooser.DelConn(connection);
		return true;
	}
	return false;
}
	}; // namespace nSocket
}; // namespace nVerliHub
