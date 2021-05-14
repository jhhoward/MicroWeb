
/*

   mTCP Eth.cpp
   Copyright (C) 2005-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Eth data structures and code (one day?)

   Changes:

   2011-05-27: Initial release as open source software

*/



// Storage for Eth.  We don't do much at the Ethernet level, so this is
// pretty thin.

#include <stdio.h>

#include "Eth.h"


EthAddr_t const Eth::Eth_Broadcast = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

EthAddr_t MyEthAddr;


// Minimum MTU for Ethernet is 46.  Maximum if 1500.  If you don't
// know about the networks in between you and the target, 576 is the
// safe default.

uint16_t  MyMTU = ETH_MTU_SAFE;

