/*
 * 
 *  Copyright ( c ) 2011
 *  name : mhogo mchungu 
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

#include "includes.h"

static int zuluCryptEXEGetDevice( const char * device )
{
	char * c = zuluCryptVolumeDeviceName( device ) ;
	if( c == NULL ){
		printf( "ERROR: could not get device address from mapper address\n" ) ;
		return 1 ;
	}else{
		printf( "%s\n",c ) ;		
		free( c ) ;
		return 0 ;
	}
}

static int zuluCryptEXECheckIfLuks( const char * device )
{
	int status = zuluCryptVolumeIsLuks( device ) ;
	
	if( status == 0 )
		printf( "\"%s\" is a luks device\n",device ) ;
	else
		printf( "\"%s\" is not a luks device\n",device ) ;
	
	return status ;
}

static int zuluCryptEXECheckEmptySlots( const char * device )
{
	int status ;
	char * c  ;
	if( zuluCryptIsPathValid( device ) != 0 ){
		printf( "path \"%s\" does not point to a device\n",device ) ;
		status = 1 ;			
	}else{
		c = zuluCryptEmptySlots( device ) ;
		if( c == NULL ){
			printf( "device \"%s\" is not a luks device\n",device ) ;
			status = 2 ;
		}else{
			printf( "%s\n",c ) ;
			status = 0 ;
			free( c ) ;
		}		
	}
	return status ;
}

static int zuluCryptEXECheckUUID( const char * device )
{
	printf( "%s\n",device ) ;
	return 0 ;
}

static int zuluCryptEXE( struct_opts * clargs, const char * mapping_name,uid_t uid )
{
	switch( clargs->action ){
		case 'B' : return zuluCryptEXESaveAndRestoreLuksHeader( clargs,uid,LUKS_HEADER_SAVE ) ;
		case 'R' : return zuluCryptEXESaveAndRestoreLuksHeader( clargs,uid,LUKS_HEADER_RESTORE ) ;		
		case 'J' : return zuluCryptEXEOpenPlainAsMe( clargs,mapping_name,uid ) ;		
		case 'X' : return zuluCryptEXEWriteDeviceWithJunk( clargs,mapping_name,uid ) ;
		case 'w' : return zuluCryptEXECheckUUID( clargs->device ) ;
		case 'b' : return zuluCryptEXECheckEmptySlots( clargs->device ) ;
		case 'i' : return zuluCryptEXECheckIfLuks( clargs->device ) ;
		case 'P' : return zuluCryptEXEGetDevice( clargs->device ) ;
		case 's' : return zuluCryptEXEVolumeInfo( mapping_name,clargs->device,uid ) ;
		case 'q' : return zuluCryptEXECloseVolume( clargs->device,mapping_name,uid ) ;
		case 'o' : return zuluCryptEXEOpenVolume( clargs,mapping_name,uid ) ;
		case 'O' : return zuluCryptEXEOpenVolume( clargs,mapping_name,uid ) ;		
		case 'c' : return zuluCryptEXECreateVolume( clargs,mapping_name,uid ) ;
		case 'a' : return zuluCryptEXEAddKey( clargs,uid ) ;
		case 'r' : return zuluCryptEXERemoveKey( clargs,uid );
		case 'E' : return zuluCryptExeFileEncrypt( clargs,mapping_name,uid ) ;
		case 'D' : return zuluCryptExeFileDecrypt( clargs,mapping_name,uid ) ;
	}
	printf("ERROR!!!!!!!!!!: cli option missed!\n" );
	return 200 ; /* shouldnt get here */	
}

//stringList_t get_partition_from_crypttab( void ) ;

int main( int argc,char * argv[] )
{
	const char * device ;
	const char * mapping_name ;
	char * ac ;
	char action ;
	int st ;
	uid_t uid ;
	
	string_t q = StringVoid ;
	
	struct_opts clargs ;

	if( argc == 1 ){
		zuluCryptEXEHelp();
		return 1;
	}
	if( argc == 2 ){
		ac = argv[ 1 ] ;
		if ( strcmp( ac,"-h" ) == 0 || strcmp( ac,"--help" ) == 0 || strcmp( ac,"-help" ) == 0 ){			
			zuluCryptEXEHelp();	
			return 0 ;
		}	
		if ( strcmp( ac,"-v" ) == 0 || strcmp( ac,"-version" ) == 0 || strcmp( ac,"--version" ) == 0 ){		
			printf( "%s\n",zuluCryptVersion() );
			return 0 ;
		}
	}
	
	zuluCryptEXEGetOpts( argc,argv,&clargs );
	
	uid = getuid();
	
	setuid( 0 );
	
	action = clargs.action ;
	device = clargs.device ;
	
	/*
	 * below tests are here because they do not use -d option
	 */	
	switch( action ){
		case 'A':
		case 'N':
		case 'S': return zuluCryptPrintPartitions( clargs.partition_number ) ;	
		case 'L': return zuluCryptPrintOpenedVolumes( uid ) ;
	}
	if( action == '\0' ){
		printf("ERROR: \"action\" argument is missing\n" ) ;
		return 130 ;
	}
	if( device == NULL ){
		printf("ERROR: required option( device path ) is missing for this operation\n" ) ;
		return 120 ;
	}
	if( strncmp( device,"UUID=",5 ) == 0 ){

		q = String( device ) ;	
		StringRemoveString( q,"\"" ) ;
		StringSubChar( q,4,'-' ) ;
		
		mapping_name = StringContent( q ) ;

		ac = zuluCryptDeviceFromUUID( mapping_name + 5 ) ;
		
		if( ac != NULL ) {		
			clargs.device = ac ;			
			st = zuluCryptEXE( &clargs,mapping_name,uid );			
			free( ac ) ;
			StringDelete( &q ) ;
			return st ;
		}else{
			printf("ERROR: could not find any partition with the presented UUID\n") ;
			StringDelete( &q ) ;
			return 110 ;			
		}	
	}else{
		if ( ( ac = strrchr( device,'/' ) ) != NULL ) {
			mapping_name =  ac + 1  ;
		}else{
			mapping_name =  device  ;			
		}
	}

	return zuluCryptEXE( &clargs,mapping_name,uid ) ;	
} 
