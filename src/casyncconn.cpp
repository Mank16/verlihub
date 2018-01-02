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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ostream>
#include "casyncsocketserver.h"
#include "cserverdc.h"

#if HAVE_ERRNO_H
	#include <errno.h>
#endif

#include "casyncconn.h"
#include "cprotocol.h"
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>        /* for sockaddr_in */
#include <sys/socket.h>        /* for AF_INET */
#include <netdb.h>             /* for gethostbyaddr */
#define sockoptval_t int
	
inline int closesocket(int s)
{
	return ::close(s);
}	

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "ctime.h"
#include "stringutils.h"

#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif

using namespace std;

namespace nVerliHub {
	using namespace nUtils;
	using namespace nEnums;
	namespace nSocket {

char *cAsyncConn::msBuffer = new char[MAX_MESS_SIZE + 1];
unsigned long cAsyncConn::sSocketCounter = 0;

cAsyncConn::cAsyncConn(int desc, cAsyncSocketServer *s, tConnType ct, bool tIPv6):
	cObj("cAsyncConn"),
	mZlibFlag(false),
	mWritable(true),
	mSockDesc(desc),
	mxServer(s),
	mxMyFactory(NULL),
	mxAcceptingFactory(NULL),
	mxProtocol(NULL),
	mpMsgParser(NULL),
	mAddrPort(0),
	mServPort(0),
	mType(ct),
	mIPv6(tIPv6)
{
	mMaxBuffer = MAX_SEND_SIZE;
	struct sockaddr saddr;
	socklen_t addr_size;
	addr_size = sizeof(saddr);
	ClearLine();
	mBufEnd = mBufReadPos = 0;

	if (mSockDesc > INVALID_SOCKET) {
		if (getpeername(mSockDesc, &saddr, &addr_size) == -1) {
			if (Log(2))
				LogStream() << "Error getting peer name, closing" << endl;

			CloseNow();
		}
		char ipstr[INET6_ADDRSTRLEN];
		// deal with both IPv4 and IPv6:
		if (saddr.sa_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)&saddr;
			inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
			mAddrIP = ipstr;
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&saddr;
			inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
			mAddrIP6 = ipstr;
		}

		//ifup local
		struct ifaddrs *ifaddr= NULL,*ifa = NULL;
		void *tmp = NULL;
		getifaddrs(&ifaddr);
		for(ifa = ifaddr;ifa!= NULL;ifa = ifa->ifa_next)
		{
			if(!ifa->ifa_addr)
				continue;
				
			if(ifa->ifa_addr->sa_family == AF_INET)
			{
				tmp = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
				char address[INET_ADDRSTRLEN];
				inet_ntop(AF_INET,tmp,address, INET_ADDRSTRLEN);
				res.push_back(address);
			}
			else if(ifa->ifa_addr->sa_family == AF_INET6)
			{
				tmp = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
				char address[INET_ADDRSTRLEN];
				inet_ntop(AF_INET6,tmp,address, INET_ADDRSTRLEN);
				res_iv6.push_back(address);
			}	
			
		}
		
		if(mxServer && mxServer->mUseDNS) // host name
			DNSLookup();

		mAddrPort = ntohs(((sockaddr_in*)&saddr)->sin_port); // port number

		if (getsockname(mSockDesc, &saddr, &addr_size) == 0) { // get server address and port that user is connected to
			
			char *mTmp = new char[INET6_ADDRSTRLEN+1];
			inet_ntop(mIPv6 ? AF_UNSPEC : AF_INET,&((struct sockaddr_in*)&saddr)->sin_addr,mTmp,sizeof(mTmp));
			if(mTmp) {
				mServAddr = strdup(mTmp);
				free(mTmp);
				LogStream() << "freeing tmp of mServAddrs" << endl;//probaly not need later?
			}
			
			mServPort =  ntohs(((sockaddr_in*)&saddr)->sin_port);
			
			
		} else if (Log(2)) {
			LogStream() << "Error getting socket name" << endl;
		}
	}

	memset (&mCloseAfter, 0, sizeof(mCloseAfter));
}

/** connect to given host (ip) on port */
cAsyncConn::cAsyncConn(const string &host, int port, bool udp):
	cObj("cAsyncConn"),
	mZlibFlag(false),
	// mIterator(0),
	mWritable(true),
	mSockDesc(INVALID_SOCKET),
	mxServer(NULL),
	mxMyFactory(NULL),
	mxAcceptingFactory(NULL),
	mxProtocol(NULL),
	mpMsgParser(NULL),
	mAddrPort(port),
	mType(eCT_SERVER),
	mBufEnd(0),
	mBufReadPos(0),
	mCloseAfter(0, 0)
{
	mMaxBuffer = MAX_SEND_SIZE;
	ClearLine();
	
	if(udp) {
		mType = eCT_SERVERUDP;
		SetupUDP(host, port);
	}
	else 
		Connect(host,port);
}

cAsyncConn::~cAsyncConn()
{
	if(mpMsgParser)
		this->DeleteParser(mpMsgParser);
	mpMsgParser = NULL;
	this->Close();
}

void cAsyncConn::Close()
{
	if(mSockDesc < 0)
		return;
	mWritable = false;
	if(mxServer)
		mxServer->OnConnClose(this);
	TEMP_FAILURE_RETRY(closesocket(mSockDesc));
	
	if(errno != EINTR) {
		sSocketCounter--;
		
		if (Log(3))
			LogStream() << "Closing socket " << mSockDesc << endl;
	}
	else if(ErrLog(1))
		LogStream() << "Socket not closed" << endl;

}

void cAsyncConn::Flush()
{
	string empty;
	// Write the content of the buffer
	if(mBufSend.length())
		Write(empty, true);
}

int cAsyncConn::ReadLineLocal()
{
	if(!mxLine)
		throw "ReadLine with null line pointer";
	char *pos,*buf;
	buf = msBuffer + mBufReadPos;
	int len;
	len = mBufEnd - mBufReadPos;

	if(NULL == (pos = (char*)memchr(buf, mSeparator,len))) {
		if(mxLine->size()+len > mLineSizeMax) {
			CloseNow();
			return 0;
		}
		mxLine->append((char*)buf,len);
		mBufEnd = 0;
		mBufReadPos = 0;
		return len;
	}
	len = pos - buf;

	mxLine->append((char*)buf,len);
	mBufReadPos += len+1;
	meLineStatus = AC_LS_LINE_DONE;

	return len+1;
}

void cAsyncConn::SetLineToRead(string *strp,char delim, int max)
{
	if(LineStatus() != AC_LS_NO_LINE)
		throw "cAsyncConn::SetLineToRead - precondition not ok";
	if(!strp)
		throw "cAsyncConn::SetLineToRead - precondition not ok - null string pointer";
	meLineStatus = AC_LS_PARTLY;
	mLineSize = 0;
	mLineSizeMax = max;
	mxLine = strp;
	mSeparator = delim;
}

void cAsyncConn::ClearLine()
{
	meLineStatus = AC_LS_NO_LINE;
	mLineSize = 0;
	mLineSizeMax = 0;
	mSeparator = '\n';
	mxLine = NULL;
}


string * cAsyncConn::GetLine()
{
	return mxLine;
}

void cAsyncConn::CloseNice(int msec)
{
	OnCloseNice();
	mWritable = false;
	if(msec <= 0 || mBufSend.empty()) {
		CloseNow();
		return;
	}

	mCloseAfter.Get();
	mCloseAfter += int(msec);
}

void cAsyncConn::CloseNow()
{
	mWritable = false;

	if(mxServer) {
		mxServer->mConnChooser.OptOut((cConnBase*)this, eCC_ALL);
		mxServer->mConnChooser.OptIn((cConnBase*)this, eCC_CLOSE);
	}
	::shutdown(mSockDesc,2);//shutdown sock
}

int cAsyncConn::ReadAll()
{
	int buf_len = 0 , i=0, addr_len = sizeof(struct sockaddr);
	mBufReadPos = 0;
	mBufEnd = 0;
	bool udp = (this->GetType() == eCT_CLIENTUDP);

	if( !getok() || !mWritable)
		return -1;

	if(!udp) {
		while(
			((buf_len = recv(mSockDesc, msBuffer, MAX_MESS_SIZE, 0)) == -1) &&
			((errno == EAGAIN) || (errno == EINTR))
			&& (i++ <= 100)
		)	{
	    ::usleep(5);
		}
	} else {
		while(
			((buf_len = recvfrom(mSockDesc, msBuffer, MAX_MESS_SIZE, 0, (struct sockaddr *)&mAddrIN, (socklen_t *)&addr_len)) == -1) &&
			(i++ <= 100)
		) {
			::usleep(5);
		}
	}

	if(buf_len <= 0) {
		if(!udp) {
			if(buf_len == 0) {
				/* Connection closed - hung up*/
				if(Log(2))
					LogStream() << "User hung up.." << endl;
				CloseNow();
				return -1;
			} else {
				if(Log(2))
					LogStream() << "Read IO Error: " << errno << " : " << strerror(errno) << endl;
				switch(errno) {
					case ECONNRESET:/* connection reset by peer */ break;
					case ETIMEDOUT:/* connection timed out */break;
					case EHOSTUNREACH:/* No route to host */ break;
					default:break;
				} // end switch
			}
			CloseNow();
			return -1;
		}
	} else {
		// Received data
		mBufEnd = buf_len;
		mBufReadPos = 0;
		// End string
		msBuffer[mBufEnd] = '\0';
		mTimeLastIOAction.Get();
	}
	return buf_len;
}

int cAsyncConn::SendAll(const char *buf, size_t &len)
{
	size_t total = 0;        /* how many bytes we've sent */
	size_t bytesleft = len; /* how many we have left to send */
	int n = 0;
	int repetitions = 0;
	bool bUDP = (this->GetType() == eCT_SERVERUDP);

#ifndef QUICK_SEND
	while(total < len) {
		try {
			if(!bUDP) {
				n = send(mSockDesc, buf + total, bytesleft, MSG_NOSIGNAL|MSG_DONTWAIT);
			} else {
				n = sendto(mSockDesc, buf + total, bytesleft, 0, (struct sockaddr *)&mAddrIN, sizeof(struct sockaddr));
			}
    		} catch(...) {
			if(ErrLog(2))
				LogStream() << "exception in SendAll(buf," << len
					<< ") total=" << total
					<< " left=" << bytesleft
					<< " rep=" << repetitions
					<< " n=" << n << endl;
			return -1;
		}
		repetitions++;
		if(n == -1)
			break;
		total += n;
		bytesleft -= n;
	}
#else
	if(!bUDP) {
		n = send(mSockDesc, buf + total, bytesleft, 0);
	}
	else
		n = sendto(mSockDesc, buf + total, bytesleft, 0, (struct sockaddr *)&mAddrIN, sizeof(struct sockaddr));
	total = n;
#endif
	len = total; /* return number actually sent here */
	return n == -1 ? -1 : 0; /* return -1 on failure, 0 on success */
}

int cAsyncConn::SetupUDP(const string &host, int port)
{
	mSockDesc = CreateSock(true);

	if(mSockDesc == INVALID_SOCKET) {
		vhErr(1) << "Error getting socket." << endl;
		return INVALID_SOCKET;
	}

	struct hostent *he = gethostbyname(host.c_str());
	if(he != NULL) {
		memset(&mAddrIN, 0, sizeof(struct sockaddr_in));
		mAddrIN.sin_family = GetIsIpv6() ? AF_UNSPEC : AF_INET;
		mAddrIN.sin_port = htons(port);
		mAddrIN.sin_addr = *((struct in_addr *)he->h_addr);
		memset(&(mAddrIN.sin_zero), '\0', 8);
		return 0;
	} else {
		vhErr(2) << "Error resolving host " << host << endl;
		return INVALID_SOCKET;
	}
}

int cAsyncConn::SendUDPMsg(const string &host, int port, const string &data)
{
	int result;
	cAsyncConn conn(host, port, true);
	if (conn.mSockDesc > INVALID_SOCKET)
		result = conn.Write(data, true);
	else
		return -1;
	if(conn.mSockDesc != INVALID_SOCKET)
		conn.Close();
	return result;
}

int cAsyncConn::Connect(const string &host, int port)
{
	struct sockaddr_in dest_addr;
	mSockDesc = CreateSock();

	if(mSockDesc == INVALID_SOCKET) {
		vhErr(1) << "Error getting socket." << endl;
		return INVALID_SOCKET;
	}
	cTime timeout(5.0);
	SetSockOpt(SO_RCVTIMEO, &timeout, sizeof(timeval));
	SetSockOpt(SO_SNDTIMEO, &timeout, sizeof(timeval));

	struct hostent *he = gethostbyname(host.c_str());
	if(he != NULL) {
		dest_addr.sin_family = GetIsIpv6() ? AF_UNSPEC : AF_INET;
		dest_addr.sin_port = htons(port);
		dest_addr.sin_addr.s_addr = *(unsigned*)(he->h_addr_list[0]);//inet_addr(host.c_str());
		memset(&(dest_addr.sin_zero), '\0', 8);

		int s = connect(mSockDesc, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
		if(s == INVALID_SOCKET) {
			vhErr(1) << "Error connecting to " << host << ":" << port << endl;
			return INVALID_SOCKET;
		}
		return mSockDesc;
	} else {
		vhErr(2) << "Error resolving host " << host << endl;
		return INVALID_SOCKET;
	}
}

int cAsyncConn::SetSockOpt(int optname, const void *optval, int optlen)
{
	return setsockopt(this->mSockDesc, SOL_SOCKET, optname, optval , optlen);
}

int cAsyncConn::GetSockOpt(int optname, void *optval, int &optlen)
{
	int result = 0;
	socklen_t _optlen;
	result = getsockopt(this->mSockDesc, SOL_SOCKET, optname, optval , &_optlen);
	return result;
}

tSocket cAsyncConn::CreateSock(bool udp,bool ipv6)
{
	tSocket sock;
	sockoptval_t yes = 1;

	if(!udp) {
		/* Create tcp socket */
		if((sock = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
			return INVALID_SOCKET;

		/* Fix the address already in use error */
		if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
			LogStream() << "set sock err " << sock << endl;
			closesocket(sock);
			return INVALID_SOCKET;
		}
	} else {
		/* Create udp socket */
		if((sock = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
			return INVALID_SOCKET;
	}

	sSocketCounter++;
	if(Log(3))
		LogStream() << "New socket " << sock << endl;

	return sock;
}

int cAsyncConn::BindSocketV6(int sockfd, int port, int type)
{
	struct addrinfo hints,*res;
	
	memset(&hints,0, sizeof(hints));
	
	hints.ai_family = AF_INET6;
	hints.ai_socktype = type;
	hints.ai_flags = AI_PASSIVE;
	char buf[33];
	sprintf(buf,"%d",port);
	getaddrinfo(NULL,buf,&hints,&res);
	
	int ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
	if(ret == -1)
	{
		LogStream() << "bind v6" << ret << endl;	
		close(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}

int cAsyncConn::BindSocket(int sockfd, int port,  char *addr,int type)
{
	if(sockfd < 0)
		return INVALID_SOCKET;
		
	struct sockaddr_in sin;
	memset(&sin,0,sizeof(sockaddr_in));	
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr =  htonl(INADDR_ANY); //listen on any aviable conn
    
    
    if (bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        fprintf(stderr, "listen_server:: could not bind socket erno %d\n",errno);
        LogStream() << "bind v4" << errno << endl;
        close(sockfd);
        return INVALID_SOCKET;
    }

	return sockfd;
}

int cAsyncConn::ListenSock(int sock)
{
	if(sock < 0)
		return INVALID_SOCKET;
	if(listen(sock, 100) == -1) {
		vhErr(0) << "Error listening" << endl;
		return INVALID_SOCKET;
	}
	return sock;
}

tSocket cAsyncConn::NonBlockSock(int sock)
{
	if(sock < 0)
		return INVALID_SOCKET;
	int flags;
	if((flags = fcntl(sock, F_GETFL, 0)) < 0)
		return INVALID_SOCKET;
	if( fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0 )
		return INVALID_SOCKET;
	return sock;
}
//true if fail
int cAsyncConn::ListenOnPort(unsigned int port, char *address, bool udp,bool ipv6)
{
	if(!(mSockDesc >= 0))
		return INVALID_SOCKET;
	mSockDesc = CreateSock(udp,ipv6);
	if(!(mSockDesc >= 0)) {
		LogStream() << "error create" << mSockDesc << endl;
		return INVALID_SOCKET;
	}	
		
	if(!ipv6) {
		mSockDesc = BindSocket(mSockDesc,port,address,udp ? SOCK_DGRAM : SOCK_STREAM);
		if(!(mSockDesc >= 0)) {
			LogStream() << "error bind" << mSockDesc << endl;
			return INVALID_SOCKET;	
		}	
	}
	if(ipv6)
	{
		mSockDesc = BindSocketV6(mSockDesc,port,udp ? SOCK_DGRAM : SOCK_STREAM);
		if(!(mSockDesc >= 0)) {
			LogStream() << "Error on bind for IPv6" << mSockDesc << endl;
			return INVALID_SOCKET;	
		}	
	}
	
	if(!udp) {
	    mSockDesc = ListenSock(mSockDesc);
		if(!(mSockDesc >= 0)) {
			LogStream() << "Error on bind for IPv4" << mSockDesc << endl;
			return INVALID_SOCKET;
		}
	    
	    mSockDesc = NonBlockSock(mSockDesc);
			if(!(mSockDesc >= 0)) {
				LogStream() << "Error on NonBlockSock Set" << mSockDesc << endl;
				return INVALID_SOCKET;	
			}	
	}
	return mSockDesc;
}

tSocket cAsyncConn::AcceptSock()
{
	socklen_t namelen;
	sockoptval_t yes = 1;
	int i=0;
	#if ! defined _WIN32
	struct sockaddr_in client;
	#else
	struct sockaddr client;
	#endif

	/* Get a socket for the connected user.  */
	namelen = sizeof(client);
	memset(&client, 0, namelen);

	tSocket socknum = ::accept(mSockDesc, (struct sockaddr *)&client, &namelen);

	while(( socknum == INVALID_SOCKET) && ((errno == EAGAIN) || (errno == EINTR)) && (i++ < 10)) {
		socknum = ::accept(mSockDesc, (struct sockaddr *)&client, (socklen_t*)&namelen);
		::usleep(50);
	}

	if(socknum == INVALID_SOCKET) {
		vhErr(1) << "Err no" << errno << endl;
		return INVALID_SOCKET;
	}
	if(Log(3))
		LogStream() << "Accepted Socket " << socknum << endl;
	sSocketCounter++;

	if(setsockopt(socknum, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) == SOCKET_ERROR) {
		TEMP_FAILURE_RETRY(closesocket(socknum));
		if(errno != EINTR) {
			sSocketCounter --;
			if (Log(3))
				LogStream() << "Closing socket " << socknum << endl;
		} else if(ErrLog(1))
			LogStream() << "Socket not closed" << endl;
		return INVALID_SOCKET;
	}
	if((socknum = NonBlockSock(socknum)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	return socknum;
}

cConnFactory * cAsyncConn::GetAcceptingFactory()
{
	if(this->mxAcceptingFactory)
		return mxAcceptingFactory;
	if(mxServer && mxServer->mFactory)
		return mxServer->mFactory;
	return NULL;
}

cAsyncConn * cAsyncConn::Accept()
{
	tSocket sd;
	cConnFactory *AcceptingFactory = NULL;
	cAsyncConn *new_conn = NULL;

	sd = AcceptSock();
	if(sd == INVALID_SOCKET)
		return NULL;
	mTimeLastIOAction.Get();

	AcceptingFactory = this->GetAcceptingFactory();
	if (AcceptingFactory != NULL)
		new_conn = AcceptingFactory->CreateConn(sd);
	if(!new_conn)
		throw "can't create connection";

	return new_conn;
}

const tConnType& cAsyncConn::getType()
{
	return mType;
}

tConnType cAsyncConn::GetType()
{
	return mType;
}

int cAsyncConn::OnTimerBase(cTime &now)
{

	if(bool(mCloseAfter) && (mCloseAfter > now)) {
		CloseNow();
		return 0;
	}
	Flush();
	OnTimer(now);
	return 0;
}

int cAsyncConn::OnTimer(cTime &now)
{
	return 0;
}

void cAsyncConn::OnFlushDone()
{ }

int cAsyncConn::Write(const string &data, bool Flush)
{
	if ((mBufSend.size() + data.size()) >= mMaxBuffer) { // append data to older data in buffer only if there is free space
		if (Log(2))
			LogStream() << "Buffer is too big, closing" << endl;

		CloseNow();
		return -1;
	}

	Flush = (Flush || (mBufSend.size() > (mMaxBuffer >> 1)));
	const char *send_buffer, *zlib_buffer; // pointer to buffers for data to send
	size_t send_size; // size of buffer
	bool appended; // data is added to old buffer content

	if (mBufSend.size() || !Flush) { // check if we have to append data to buffer or send it immediatly
		mBufSend.append(data.data(), data.size());
		send_buffer = mBufSend.data();
		send_size = mBufSend.size();
		appended = true;
	} else {
		send_buffer = data.data();
		send_size = data.size();
		appended = false;
	}

	if (!send_size) // check if there is anything to send
		return 0;

	if (!Flush) // we have appended data, so do not send now
		return 0;

	bool compressed = false;

	if (mZlibFlag && mxServer) { // compress data with zlib, only when flushing, otherwise we will destroy everything
		nVerliHub::cServerDC *serv = (nVerliHub::cServerDC*)mxServer;

		if (serv && !serv->mC.disable_zlib && (send_size >= serv->mC.zlib_min_len)) { // only when enabled and minimum length is reached
			if (send_buffer[send_size - 1] == '|') {
				size_t zlib_size = 0;
				zlib_buffer = serv->mZLib->Compress(send_buffer, send_size, zlib_size);

				if (zlib_size && zlib_buffer) {
					send_buffer = zlib_buffer;
					send_size = zlib_size;
					compressed = true;
				} else if (Log(5)) {
					if (zlib_size)
						LogStream() << "Compressed ZLib data is larger, fall back" << endl;
					else
						LogStream() << "Failed compressing data with ZLib, fall back" << endl;
				}
			} else if (Log(1)) // client will fail to decompress when pipe is missing
				LogStream() << "Missing ending pipe in compress data: " << send_buffer << endl;
		}
	}

	size_t size_sent = send_size; // make copy of send_size, because SendAll method will change it

	if (SendAll(send_buffer, size_sent) == -1) { // send the data as much as possible
		if (Log(6) && mxServer) {
			nVerliHub::cServerDC *serv = (nVerliHub::cServerDC*)mxServer;

			if (serv && serv->mNetOutLog && serv->mNetOutLog.is_open()) {
				serv->mNetOutLog << "[" << AddrIP() << "] Error sending " << ((compressed) ? "compressed" : "plain") << " data: " << ((appended) ? mBufSend : data) << " (size: " << send_size << ", sent: " << size_sent << ")" << endl;
				serv->mNetOutLog << "Error " << errno << ": " << strerror(errno) << endl;
			}
		}

		if ((errno != EAGAIN) && (errno != EINTR)) { // analyze the error
			if (Log(2))
				LogStream() << "Error during writing, closing" << endl;

			CloseNow(); // error during writing, remove the user
			return -1;
		}

		if (size_sent > 0) { // if something has been sent, but not all, update the buffer
			mTimeLastIOAction.Get();

			if (compressed) { // todo: for now we get rid of compressed unsent data, change that later
				if (appended)
					mBufSend.clear();
			} else { // this is supposed to actually reduce the size of mBufSend, it does a copy so it is slower but memory usage is important thing
				if (appended) // if we appended, assign the rest of string to the buffer
					StrCutLeft(mBufSend, size_sent);
				else
					StrCutLeft(data, mBufSend, size_sent);
			}
		} else {
			if (bool(mCloseAfter)) // close nice was called, we must close the connection even if the data cannot be transmitted
				CloseNow();
		}

		if (mxServer && (mSockDesc > INVALID_SOCKET)) { // buffer overfill protection, only on registered connections
			mxServer->mConnChooser.OptIn(this, eCC_OUTPUT); // choose the connection to send the rest of data as soon as possible

			if (mBufSend.size() < MAX_SEND_UNBLOCK_SIZE) { // if buffer size is lower then UNBLOCK size, allow read operation on the connection
				mxServer->mConnChooser.OptIn(this, eCC_INPUT);

				if (Log(5)) {
					nVerliHub::cServerDC *serv = (nVerliHub::cServerDC*)mxServer;

					if (serv && serv->mNetOutLog && serv->mNetOutLog.is_open())
						serv->mNetOutLog << "Unblocking read operation on socket" << endl;

					LogStream() << "Unblock INPUT" << endl;
				}
			} else if (mBufSend.size() >= MAX_SEND_FILL_SIZE) { // if buffer is bigger than max send size, block read operation
				mxServer->mConnChooser.OptOut(this, eCC_INPUT);

				if (Log(5)) {
					nVerliHub::cServerDC *serv = (nVerliHub::cServerDC*)mxServer;

					if (serv && serv->mNetOutLog && serv->mNetOutLog.is_open())
						serv->mNetOutLog << "Blocking read operation on socket" << endl;

					LogStream() << "Block INPUT" << endl;
				}
			}
		}
	} else { // all data has been sent
		if (appended) // clear output buffer
			mBufSend.erase(0, mBufSend.size());

		ShrinkStringToFit(mBufSend);

		if (bool(mCloseAfter)) // close nice was called, close the connection
			CloseNow();

		if (mxServer && (mSockDesc > INVALID_SOCKET) ) { // unregister the connection for write operation
			mxServer->mConnChooser.OptOut(this, eCC_OUTPUT);

			if (Log(5))
				LogStream() << "Block OUTPUT" << endl;
		}

		mTimeLastIOAction.Get();
		OnFlushDone(); // report flush is done
	}

	return size_sent;
}

int cAsyncConn::OnCloseNice(void)
{
	return 0;
}

cMessageParser *cAsyncConn::CreateParser()
{
	if (this->mxProtocol != NULL)
		return this->mxProtocol->CreateParser();
	else return NULL;
}

void cAsyncConn::DeleteParser(cMessageParser *OldParser)
{
	if (this->mxProtocol != NULL)
		this->mxProtocol->DeleteParser(OldParser);
	else
		delete OldParser;
}

string * cAsyncConn::FactoryString()
{
	if(mpMsgParser == NULL)
		mpMsgParser = this->CreateParser();
	if(mpMsgParser == NULL)
		return NULL;
	mpMsgParser->ReInit();
	return &(mpMsgParser->GetStr());
}


bool cAsyncConn::DNSLookup()
{
	if(mAddrHost.empty())
	    	return true;
	struct hostent *hp;
	unsigned int addr;
	addr = inet_addr(mAddrIP.c_str());
	
	if((hp=gethostbyaddr((char *)&addr,sizeof(addr),AF_UNSPEC)))
		mAddrHost = hp->h_name;
	return (hp != NULL);
}

unsigned long cAsyncConn::DNSResolveHost(const string &host)
{
	struct sockaddr_in AddrIN;
	memset(&AddrIN, 0, sizeof(sockaddr_in));
	struct hostent *he = gethostbyname(host.c_str());
	if(he != NULL) {
		AddrIN.sin_addr = *((struct in_addr *)he->h_addr);
	}
	return AddrIN.sin_addr.s_addr;
}

bool cAsyncConn::DNSResolveReverse(const string &ip, string &host)
{
	struct hostent *hp;
	struct in_addr addr;
	if(!inet_aton(ip.c_str(), &addr))
		return false;
	if((hp = gethostbyaddr((char *)&addr,sizeof(addr),AF_UNSPEC)))
		host=hp->h_name;
	return hp != NULL;
}

string cAsyncConn::IPAsString(unsigned long addr)
{
	struct in_addr in;
	in.s_addr = addr;
	char ip[INET_ADDRSTRLEN+1];
	inet_ntop(AF_UNSPEC, (const void*) &in, ip, INET_ADDRSTRLEN);
	return string(ip);
}


cAsyncConn * cConnFactory::CreateConn(tSocket sd)
{
	cAsyncConn *conn = new cAsyncConn(sd);
	conn->mxMyFactory = this;
	return conn;
}

void cConnFactory::DeleteConn(cAsyncConn * &conn)
{
	conn->Close();
	delete conn;
	conn = NULL;
}
	}; // namespace nSocket
}; // namespace nVerliHub
