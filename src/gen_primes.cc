/*******************************************************************************
  gen_primes.cc, generates some constant numbers (arrays) of mpz_sprime.cc

   This file is part of LibTMCG.

 Copyright (C) 2018, 2019  Heiko Stamer <HeikoStamer@gmx.net>

   LibTMCG is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   LibTMCG is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with LibTMCG; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#include <cstdio>
#include <cassert>
#include <iostream>

int main
	()
{
	unsigned long int b = 80000;
	unsigned long int ps = 0, ls = 0;
	std::cout << "unsigned long int primes[] = {" << std::endl;
	for (unsigned long int i = 3; i < b; i++)
	{
		bool prime = true;
		for (unsigned long int j = 2; (j < i) && (j < b); j++)
		{
			if ((i % j) == 0)
			{
				prime = false;
				break;
			}
		}
		if (prime)
		{
			++ps;
			std::cout << i << ", ";
			if (++ls == 8)
			{
				std::cout << std::endl << "\t";
				ls = 0;
			}
		}
	}
	std::cout << "0" << std::endl << "}; // PRIMES_SIZE = " << ps << std::endl;
	ls = 0;
	std::cout << "unsigned long int primes_m1d2[] = {" << std::endl;
	for (unsigned long int i = 3; i < b; i++)
	{
		bool prime = true;
		for (unsigned long int j = 2; (j < i) && (j < b); j++)
		{
			if ((i % j) == 0)
			{
				prime = false;
				break;
			}
		}
		if (prime)
		{
			std::cout << ((i - 1)/2) << ", ";
			if (++ls == 8)
			{
				std::cout << std::endl << "\t";
				ls = 0;
			}
		}
	}
	std::cout << "0" << std::endl << "};" << std::endl;	
	return 0;
}
