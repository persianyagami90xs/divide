/* Copyright (C) 2004 Bart
 * Copyright (C) 2008, 2009, 2010 Curtis Gedak
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
 
#include "../include/fat16.h"

/*****
//For some reason unknown, this works without these include statements.
#include <stdlib.h>    // 'C' library for mkstemp()
#include <unistd.h>    // 'C' library for write(), close()
#include <stdio.h>     // 'C' library for remove()
*****/

namespace GParted
{

FS fat16::get_filesystem_support()
{
	FS fs ;
	fs .filesystem = GParted::FS_FAT16 ;
		
	//find out if we can create fat16 file systems
	if ( ! Glib::find_program_in_path( "mkdosfs" ) .empty() )
		fs .create = GParted::FS::EXTERNAL ;
	
	if ( ! Glib::find_program_in_path( "dosfsck" ) .empty() )
	{
		fs .check = GParted::FS::EXTERNAL ;
		fs .read = GParted::FS::EXTERNAL ;
	}

	if ( ! Glib::find_program_in_path( "mlabel" ) .empty() ) {
		fs .read_label = FS::EXTERNAL ;
		fs .write_label = FS::EXTERNAL ;
	}

	//resizing of start and endpoint are provided by libparted
	fs .grow = GParted::FS::LIBPARTED ;
	fs .shrink = GParted::FS::LIBPARTED ;
	fs .move = GParted::FS::LIBPARTED ;
		
	fs .copy = GParted::FS::GPARTED ;
	
	fs .MIN = 16 * MEBIBYTE ;
	fs .MAX = (4096 - 1) * MEBIBYTE ;  //Maximum seems to be just less than 4096 MiB
	
	return fs ;
}

void fat16::set_used_sectors( Partition & partition ) 
{
	exit_status = Utils::execute_command( "dosfsck -n -v " + partition .get_path(), output, error, true ) ;
	if ( exit_status == 0 || exit_status == 1 || exit_status == 256 )
	{
		//free clusters
		index = output .find( ",", output .find( partition .get_path() ) + partition .get_path() .length() ) +1 ;
		if ( index < output .length() && sscanf( output .substr( index ) .c_str(), "%Ld/%Ld", &S, &N ) == 2 ) 
			N -= S ;
		else
			N = -1 ;

		//bytes per cluster
		index = output .rfind( "\n", output .find( "bytes per cluster" ) ) +1 ;
		if ( index >= output .length() || sscanf( output .substr( index ) .c_str(), "%Ld", &S ) != 1 )
			S = -1 ;
	
		if ( N > -1 && S > -1 )
			partition .Set_Unused( Utils::round( N * ( S / double(partition .sector_size) ) ) ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

void fat16::read_label( Partition & partition )
{
	//Create mtools config file
	char fname[] = "/tmp/gparted-XXXXXXXX" ;
	char dletter = 'H' ;
	Glib::ustring err_msg = "" ;
	err_msg = Utils::create_mtoolsrc_file( fname, dletter, partition.get_path() ) ;
	if( err_msg.length() != 0 )
		partition .messages .push_back( err_msg );

	Glib::ustring cmd = String::ucompose( "export MTOOLSRC=%1 && mlabel -s %2:", fname, dletter ) ;

	if ( ! Utils::execute_command( cmd, output, error, true ) )
	{
		partition .label = Utils::trim( Utils::regexp_label( output, "Volume label is ([^(]*)" ) ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;
		
		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
	
	//Delete mtools config file
	err_msg = Utils::delete_mtoolsrc_file( fname );
}

bool fat16::write_label( const Partition & partition, OperationDetail & operationdetail )
{
	//Create mtools config file
	char fname[] = "/tmp/gparted-XXXXXXXX" ;
	char dletter = 'H' ;
	Glib::ustring err_msg = "" ;
	err_msg = Utils::create_mtoolsrc_file( fname, dletter, partition.get_path() ) ;

	Glib::ustring cmd = "" ;
	if( partition .label .empty() )
		cmd = String::ucompose( "export MTOOLSRC=%1 && mlabel -c %2:", fname, dletter ) ;
	else
		cmd = String::ucompose( "export MTOOLSRC=%1 && mlabel %2:\"%3\"", fname, dletter, Utils::fat_compliant_label( partition .label ) ) ;
	
	operationdetail .add_child( OperationDetail( cmd, STATUS_NONE, FONT_BOLD_ITALIC ) ) ;
	
	int exit_status = Utils::execute_command( cmd, output, error ) ;

	if ( ! output .empty() )
		operationdetail .get_last_child() .add_child( OperationDetail( output, STATUS_NONE, FONT_ITALIC ) ) ;

	if ( ! error .empty() )
		operationdetail .get_last_child() .add_child( OperationDetail( error, STATUS_NONE, FONT_ITALIC ) ) ;

	//Delete mtools config file
	err_msg = Utils::delete_mtoolsrc_file( fname );

	return ( exit_status == 0 );
}

bool fat16::create( const Partition & new_partition, OperationDetail & operationdetail )
{
	return ! execute_command( "mkdosfs -F16 -v -n \"" + Utils::fat_compliant_label( new_partition .label ) + "\" " + new_partition .get_path(), operationdetail ) ;
}

bool fat16::resize( const Partition & partition_new, OperationDetail & operationdetail, bool fill_partition )
{
	return true ;
}

bool fat16::move( const Partition & partition_new
                , const Partition & partition_old
                , OperationDetail & operationdetail
                )
{
	return true ;
}

bool fat16::copy( const Glib::ustring & src_part_path,
		  const Glib::ustring & dest_part_path,
		  OperationDetail & operationdetail )
{
	return true ;
}

bool fat16::check_repair( const Partition & partition, OperationDetail & operationdetail )
{
	exit_status = execute_command( "dosfsck -a -w -v " + partition .get_path(), operationdetail ) ;

	return ( exit_status == 0 || exit_status == 1 || exit_status == 256 ) ;
}

} //GParted


