/*******************************************************************************
   libTMCG.hh, general header of the |T|oolbox for |M|ental |C|ard |G|ames

   This file is part of libTMCG.

 Copyright (C) 2004, 2005  Heiko Stamer <stamer@gaos.org>

   libTMCG is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   libTMCG is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libTMCG; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*******************************************************************************/

#ifndef INCLUDED_libTMCG_HH
	#define INCLUDED_libTMCG_HH
	
	#include <libTMCG_config.h>

	#include <TMCG_SecretKey.hh>
	#include <TMCG_PublicKey.hh>
	#include <TMCG_PublicKeyRing.hh>

	#include <VTMF_Card.hh>
	#include <VTMF_CardSecret.hh>
	#include <TMCG_Card.hh>
	#include <TMCG_CardSecret.hh>
	#include <TMCG_Stack.hh>
	#include <TMCG_OpenStack.hh>
	#include <TMCG_StackSecret.hh>
	
	#include <SchindelhauerTMCG.hh>
	
	bool init_libTMCG
		()
	{
		// initalize libgcrypt
		if (!gcry_check_version(TMCG_LIBGCRYPT_VERSION))
		{
			std::cerr << "libgcrypt: need library version >= " <<
				TMCG_LIBGCRYPT_VERSION << std::endl;
			return false;
		}
		gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
		gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
		if (gcry_md_test_algo(TMCG_GCRY_MD_ALGO))
		{
			std::cerr << "libgcrypt: algorithm " << TMCG_GCRY_MD_ALGO <<
				" [" << gcry_md_algo_name(TMCG_GCRY_MD_ALGO) <<
				"] not available" << std::endl;
			return false;
		}
		return true;
	}
#endif
