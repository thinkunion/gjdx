//
// EchoServer.cpp
//
// $Id: //poco/1.4/Net/samples/EchoServer/src/EchoServer.cpp#1 $
//
// This sample demonstrates the SocketReactor and SocketAcceptor classes.
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/NObserver.h"
#include "Poco/Exception.h"
#include "Poco/Thread.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Timestamp.h"
#include <iostream>
#include "ss7Server.h"


using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Thread;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;
using Poco::NumberFormatter;
using Poco::Timestamp;
using Poco::Timespan;

class EchoServiceHandler
{
public:
	EchoServiceHandler(StreamSocket& socket, SocketReactor& reactor):
		_socket(socket),
		_reactor(reactor),
		_recvBuffer(new unsigned char[BUFFER_SIZE]),
		_sendBuffer(new unsigned char[BUFFER_SIZE])
	{
		Application& app = Application::instance();
		
		Timespan ts(20,0);
		socket.setReceiveTimeout(ts);

		app.logger().information("Connection from " + socket.peerAddress().toString());

		_reactor.addEventHandler(_socket, NObserver<EchoServiceHandler, ReadableNotification>(*this, &EchoServiceHandler::onReadable));
		_reactor.addEventHandler(_socket, NObserver<EchoServiceHandler, ShutdownNotification>(*this, &EchoServiceHandler::onShutdown));
	}
	
	~EchoServiceHandler()
	{
		Application& app = Application::instance();
		try
		{
			app.logger().information("Disconnecting " + _socket.peerAddress().toString());
		}
		catch (...)
		{
		}
		_reactor.removeEventHandler(_socket, NObserver<EchoServiceHandler, ReadableNotification>(*this, &EchoServiceHandler::onReadable));
		_reactor.removeEventHandler(_socket, NObserver<EchoServiceHandler, ShutdownNotification>(*this, &EchoServiceHandler::onShutdown));
		delete [] _recvBuffer;
		delete [] _sendBuffer;
	}
	
	void onReadable(const AutoPtr<ReadableNotification>& pNf)
	{
			UInt16	msglen = 0;
			Int32	leftlen = 0;
			Int32	recvlen = _socket.receiveBytes(_recvBuffer, BUFFER_SIZE);
	
			Application& app = Application::instance();

			if (recvlen > 0)
			{
				app.logger().information("recv a message,parse start!!! ");

				if( (_recvBuffer[0]==0x9e) && (_recvBuffer[1]==0x62) )
					{
						msglen = ntohs(*((u_short *)(_recvBuffer+2)));
						leftlen = msglen;

					}else{
						app.logger().information("ss7server message head flag lost,it should is 0x9e62 ");
						delete this;
				}

				if( msglen == (recvlen -5))
				{
					echoMessage(_recvBuffer,_sendBuffer);
					_socket.sendBytes(_sendBuffer, 11);
				}else
					app.logger().information("ss7server message length error, somethine is lost!!!");
			}
			else{
				if( recvlen == 0 )
					app.logger().information("socket timeout,recvive next time !!!");
				else{
					app.logger().information("socket error,recvive packet fail !!!");
					delete this;
				}
			}
	}
	
	void onShutdown(const AutoPtr<ShutdownNotification>& pNf)
	{
		delete this;
	}

void echoMessage(unsigned char *recvbuf,unsigned char *sendbuf){

	MSGHEAD *recvhead;
	MSGHEAD *resphead;

	Application& app = Application::instance();

	recvhead = (MSGHEAD *)recvbuf;
	recvhead->msglen = ntohs(recvhead->msglen);
	recvhead->cmd = ntohs(recvhead->cmd);
	recvhead->cmdstatus = ntohs(recvhead->cmdstatus);
	recvhead->seq = ntohs(recvhead->seq);

	memset(sendbuf,0,BUFFER_SIZE);
	sendbuf[0] = 0x9e;
	sendbuf[1] = 0x62;

	switch( recvhead->cmd )
	{
/*		case CMD_RECORD:
			applog.debug("recv Record message: sequence number is " + NumberFormatter::format(mh->seq) );
			parseCdrRecord();
			break;
*/
		case CMD_CHECKLINK:
			app.logger().debug("recv Check_Link message: sequence number is " + NumberFormatter::format(recvhead->seq) );

			resphead = (MSGHEAD *)sendbuf;
			resphead->cmd = htons(CMD_CHECKLINK_RESP);
			resphead->cmdstatus = 0;
			resphead->msglen = htons(6);
			resphead->seq = htons(recvhead->seq);

			break;
		case CMD_BIND:
			app.logger().debug("recv Bind_Resp message: sequence number is " + NumberFormatter::format(recvhead->seq) );

			resphead = (MSGHEAD *)sendbuf;
			resphead->cmd = htons(CMD_BIND_RESP);
			resphead->cmdstatus = 0;
			resphead->msglen = htons(6);
			resphead->seq = htons(recvhead->seq);

			break;
		case CMD_UNBIND:
			app.logger().debug("recv UnBind_Resp message: sequence number is " + NumberFormatter::format(recvhead->seq) );

			resphead = (MSGHEAD *)sendbuf;
			resphead->cmd = htons(CMD_UNBIND_RESP);
			resphead->cmdstatus = 0;
			resphead->msglen = htons(6);
			resphead->seq = htons(recvhead->seq);

			break;
	   default:
		   break;
	}
	
}

private:
	enum
	{
		BUFFER_SIZE = 65550
	};
	
	StreamSocket   _socket;
	SocketReactor& _reactor;
	unsigned char*	_recvBuffer;
	unsigned char*	_sendBuffer;

};


class EchoServer: public Poco::Util::ServerApplication
	/// The main application class.
	///
	/// This class handles command-line arguments and
	/// configuration files.
	/// Start the EchoServer executable with the help
	/// option (/help on Windows, --help on Unix) for
	/// the available command line options.
	///
	/// To use the sample configuration file (EchoServer.properties),
	/// copy the file to the directory where the EchoServer executable
	/// resides. If you start the debug version of the EchoServer
	/// (EchoServerd[.exe]), you must also create a copy of the configuration
	/// file named EchoServerd.properties. In the configuration file, you
	/// can specify the port on which the server is listening (default
	/// 9977) and the format of the date/time string sent back to the client.
	///
	/// To test the EchoServer you can use any telnet client (telnet localhost 9977).
{
public:
	EchoServer(): _helpRequested(false)
	{
	}
	
	~EchoServer()
	{
	}

protected:
	void initialize(Application& self)
	{
		loadConfiguration(); // load default configuration files, if present
		ServerApplication::initialize(self);
	}
		
	void uninitialize()
	{
		ServerApplication::uninitialize();
	}

	void defineOptions(OptionSet& options)
	{
		ServerApplication::defineOptions(options);
		
		options.addOption(
			Option("help", "h", "display help information on command line arguments")
				.required(false)
				.repeatable(false));
	}

	void handleOption(const std::string& name, const std::string& value)
	{
		ServerApplication::handleOption(name, value);

		if (name == "help")
			_helpRequested = true;
	}

	void displayHelp()
	{
		HelpFormatter helpFormatter(options());
		helpFormatter.setCommand(commandName());
		helpFormatter.setUsage("OPTIONS");
		helpFormatter.setHeader("An ss7 server implemented using the Reactor and Acceptor patterns.");
		helpFormatter.format(std::cout);
	}

	int main(const std::vector<std::string>& args)
	{
		if (_helpRequested)
		{
			displayHelp();
		}
		else
		{
			// get parameters from configuration file
			unsigned short port = (unsigned short) config().getInt("EchoServer.port", 9977);
			
			// set-up a server socket
			ServerSocket svs(port);
			// set-up a SocketReactor...
			SocketReactor reactor;
			// ... and a SocketAcceptor
			SocketAcceptor<EchoServiceHandler> acceptor(svs, reactor);
			// run the reactor in its own thread so that we can wait for 
			// a termination request
			Thread thread;
			thread.start(reactor);
			// wait for CTRL-C or kill
			waitForTerminationRequest();
			// Stop the SocketReactor
			reactor.stop();
			thread.join();
		}
		return Application::EXIT_OK;
	}
	
private:
	bool _helpRequested;
};


int main(int argc, char** argv)
{
	EchoServer app;
	return app.run(argc, argv);
}


