/*
 *
 *  Copyright ( c ) 2017
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "zulupolkit.h"
#include "../zuluCrypt-gui/task.h"
#include "bin_path.h"
#include "../zuluCrypt-gui/executablesearchpaths.h"
#include "3rdParty/json.hpp"

#include <termios.h>
#include <memory>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <QTimer>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>

namespace utility
{
	struct Task
	{
		static ::Task::future< utility::Task >& run( const QString& exe,
							     const QString& password )
		{
			return ::Task::run< utility::Task >( [ = ](){

				return utility::Task( exe,password ) ;
			} ) ;
		}
		Task()
		{
		}
		Task( const QString& exe,const QString& password )
		{
			QProcess p ;

			p.start( exe ) ;

			p.waitForStarted() ;

			p.write( password.toLatin1() + '\n' ) ;

			p.closeWriteChannel() ;

			finished   = p.waitForFinished( -1 ) ;
			exitCode   = p.exitCode() ;
			exitStatus = p.exitStatus() ;
			stdOut     = p.readAllStandardOutput() ;
			stdError   = p.readAllStandardError() ;
		}

		QByteArray stdOut ;
		QByteArray stdError ;

		int exitCode   = 255 ;
		int exitStatus = 255 ;
		bool finished  = false ;
	};

	QString executableFullPath( const QString& e )
	{
		QString exe ;

		for( const auto& it : executableSearchPaths::values() ){

			exe = it + e ;

			if( QFile::exists( exe ) ){

				return exe ;
			}
		}

		return QString() ;
	}
}

static bool _terminalEchoOff( struct termios * old,struct termios * current )
{
	if( tcgetattr( 1,old ) != 0 ){

		return false ;
	}

	*current = *old;
	current->c_lflag &= ~ECHO;

	if( tcsetattr( 1,TCSAFLUSH,current ) != 0 ){

		return false ;
	}else{
		return true ;
	}
}

zuluPolkit::zuluPolkit( const QStringList& s ) : m_arguments( s )
{
	connect( &m_server,SIGNAL( newConnection() ),this,SLOT( gotConnection() ) ) ;
}

zuluPolkit::~zuluPolkit()
{
	QDir().remove( m_socketPath ) ;
}

void zuluPolkit::start()
{
	if( m_arguments.size() > 1 ){

		m_cookie = this->readStdin() ;

		m_socketPath = m_arguments.at( 1 ) ;

		QDir().remove( m_socketPath ) ;

		m_server.listen( m_socketPath ) ;
	}
}

static void _respond( std::unique_ptr< QLocalSocket >& s,
		      const utility::Task& e = utility::Task() )
{
	nlohmann::json json ;

	json[ "stdOut" ]     = e.stdOut.constData() ;
	json[ "stdError" ]   = e.stdError.constData() ;
	json[ "exitCode" ]   = e.exitCode ;
	json[ "exitStatus" ] = e.exitStatus ;
	json[ "finished" ]   = e.finished ;

	s->write( json.dump().c_str() ) ;

	s->waitForBytesWritten() ;
}

static bool _correct_cmd( const QString& cmd )
{
	auto a = ZULUCRYPTzuluCrypt" " ;
	auto b = zuluMountPath" " ;
	auto c = "/bin/su - -c \"" + utility::executableFullPath( "ecryptfs-simple" ) ;

	return cmd.startsWith( a ) || cmd.startsWith( b ) || cmd.startsWith( c ) ;
}

void zuluPolkit::gotConnection()
{
	std::unique_ptr< QLocalSocket > s( m_server.nextPendingConnection() ) ;

	try{
		s->waitForReadyRead() ;

		auto json = nlohmann::json::parse( s->readAll().constData() ) ;

		auto password = QString::fromStdString( json[ "password" ].get< std::string >() ) ;
		auto cookie   = QString::fromStdString( json[ "cookie" ].get< std::string >() ) ;
		auto command  = QString::fromStdString( json[ "command" ].get< std::string >() ) ;

		if( cookie == m_cookie ){

			if( command == "exit" ){

				return QCoreApplication::quit() ;

			}else if( _correct_cmd( command ) ){

				return _respond( s,utility::Task( command,password ) ) ;
			}
		}

	}catch( ... ){}

	_respond( s ) ;
}

QString zuluPolkit::readStdin()
{
	std::cout << "Token: " << std::flush ;

	struct termios old ;
	struct termios current ;

	_terminalEchoOff( &old,&current ) ;

	QString s ;
	int e ;

	int m = 1024 ;

	for( int i = 0 ; i < m ; i++ ){

		e = std::getchar() ;

		if( e == '\n' || e == -1 ){

			break ;
		}else{
			s += static_cast< char >( e ) ;
		}
	}

	tcsetattr( 1,TCSAFLUSH,&old ) ;

	std::cout << std::endl ;

	return s ;
}
