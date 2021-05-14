/*

   mTCP EthType.H
   Copyright (C) 2015-2020 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP


   This file is part of mTCP.

   mTCP is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   mTCP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with mTCP.  If not, see <http://www.gnu.org/licenses/>.


   Description: A simple header file for the EthType.cpp file.

   Changes:

   2015-03-31: New for use with pkttool.cpp

*/


#ifndef _ETHERTYP_H
#define _ETHERTYP_H

#include "types.h"


// Given a 16 bit EtherType, return a string that describes what it is.
const char * EtherType_findDescription( EtherType target );


#endif
