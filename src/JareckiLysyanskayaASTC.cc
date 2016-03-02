/*******************************************************************************
  JareckiLysyanskayaASTC.cc,
                             |A|daptively |S|ecure |T|hreshold |C|ryptography

     Stanislaw Jarecki and Anna Lysyanskaya:
       'Adaptively Secure Threshold Cryptography: Introducing Concurrency,
        Removing Erasures', TODO.

   This file is part of LibTMCG.

 Copyright (C) 2016  Heiko Stamer <HeikoStamer@gmx.net>

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

#include "JareckiLysyanskayaASTC.hh"

JareckiLysyanskayaRVSS::JareckiLysyanskayaRVSS
	(size_t n_in, size_t t_in,
	mpz_srcptr p_CRS, mpz_srcptr q_CRS, mpz_srcptr g_CRS, mpz_srcptr h_CRS,
	unsigned long int fieldsize, unsigned long int subgroupsize):
			F_size(fieldsize), G_size(subgroupsize), n(n_in), t(t_in)
{
	mpz_init_set(p, p_CRS), mpz_init_set(q, q_CRS), mpz_init_set(g, g_CRS),
		mpz_init_set(h, h_CRS);

	mpz_init_set_ui(alpha_i, 0L), mpz_init_set_ui(hatalpha_i, 0L);
	for (size_t j = 0; j < n_in; j++)
	{
//		mpz_ptr tmp1 = new mpz_t(), tmp2 = new mpz_t();
//		mpz_init(tmp1), mpz_init(tmp1);
//		y_i.push_back(tmp1), z_i.push_back(tmp2);
		std::vector<mpz_ptr> *vtmp1 = new std::vector<mpz_ptr>;
		for (size_t i = 0; i < n_in; i++)
		{
			mpz_ptr tmp3 = new mpz_t();
			mpz_init(tmp3);
			vtmp1->push_back(tmp3);
		}
		alpha_ij.push_back(*vtmp1);
		std::vector<mpz_ptr> *vtmp2 = new std::vector<mpz_ptr>;
		for (size_t i = 0; i < n_in; i++)
		{
			mpz_ptr tmp4 = new mpz_t();
			mpz_init(tmp4);
			vtmp2->push_back(tmp4);
		}
		hatalpha_ij.push_back(*vtmp2);
	}

	// Do the precomputation for the fast exponentiation.
	fpowm_table_g = new mpz_t[TMCG_MAX_FPOWM_T]();
	fpowm_table_h = new mpz_t[TMCG_MAX_FPOWM_T]();
	mpz_fpowm_init(fpowm_table_g), mpz_fpowm_init(fpowm_table_h);
	mpz_fpowm_precompute(fpowm_table_g, g, p, mpz_sizeinbase(q, 2L));
	mpz_fpowm_precompute(fpowm_table_h, h, p, mpz_sizeinbase(q, 2L));
}

bool JareckiLysyanskayaRVSS::CheckGroup
	() const
{
	mpz_t foo, k;

	mpz_init(foo), mpz_init(k);
	try
	{
		// Compute $k := (p - 1) / q$
		mpz_set(k, p);
		mpz_sub_ui(k, k, 1L);
		mpz_div(k, k, q);

		// Check whether $p$ and $q$ have appropriate sizes.
		if ((mpz_sizeinbase(p, 2L) < F_size) ||
			(mpz_sizeinbase(q, 2L) < G_size))
				throw false;

		// Check whether $p$ has the correct form, i.e. $p = kq + 1$.
		mpz_mul(foo, q, k);
		mpz_add_ui(foo, foo, 1L);
		if (mpz_cmp(foo, p))
			throw false;

		// Check whether $p$ and $q$ are both (probable) prime with
		// a soundness error probability ${} \le 4^{-TMCG_MR_ITERATIONS}$.
		if (!mpz_probab_prime_p(p, TMCG_MR_ITERATIONS) || 
			!mpz_probab_prime_p(q, TMCG_MR_ITERATIONS))
				throw false;

		// Check whether $k$ is not divisible by $q$, i.e. $q, k$ are coprime.
		mpz_gcd(foo, q, k);
		if (mpz_cmp_ui(foo, 1L))
			throw false;

		// Check whether the elements $h$ and $g$ are of order $q$.
		mpz_fpowm(fpowm_table_h, foo, h, q, p);
		if (mpz_cmp_ui(foo, 1L))
			throw false;
		mpz_fpowm(fpowm_table_g, foo, g, q, p);
		if (mpz_cmp_ui(foo, 1L))
			throw false;

		// Check whether the elements $h$ and $g$ are different and non-trivial,
		// i.e., $1 < h, g < p-1$.
		mpz_sub_ui(foo, p, 1L); // compute $p-1$
		if ((mpz_cmp_ui(h, 1L) <= 0) || (mpz_cmp(h, foo) >= 0))
			throw false;
		if ((mpz_cmp_ui(g, 1L) <= 0) || (mpz_cmp(g, foo) >= 0))
			throw false;
		if (!mpz_cmp(g, h))
			throw false;

		// everything is sound
		throw true;
	}
	catch (bool return_value)
	{
		mpz_clear(foo), mpz_clear(k);
		return return_value;
	}
}

bool JareckiLysyanskayaRVSS::Generate
	(size_t i, aiounicast *aiou, CachinKursawePetzoldShoupRBC *rbc,
	std::ostream &err, bool simulate_faulty_behaviour)
{
	assert(t <= n);
	assert((2 * t) < n); // maximum synchronous t-resilience
	assert(i < n);
	assert(n == aiou->n);
	assert(n == rbc->n);
	assert(t == aiou->t);
	assert(t == rbc->t);
	assert(i == aiou->j);
	assert(i == rbc->j);

	// initialize
	mpz_t foo, bar, lhs, rhs;
	std::vector<mpz_ptr> c_ik, hatc_ik;
	std::vector< std::vector<mpz_ptr> > C_ik;
	std::vector<size_t> complaints, complaints_counter;

	mpz_init(foo), mpz_init(bar), mpz_init(lhs), mpz_init(rhs);
	for (size_t k = 0; k <= t; k++)
	{
		mpz_ptr tmp1 = new mpz_t(), tmp2 = new mpz_t();
		mpz_init(tmp1), mpz_init(tmp2);
		c_ik.push_back(tmp1), hatc_ik.push_back(tmp2);
	}
	for (size_t j = 0; j < n; j++)
	{
		std::vector<mpz_ptr> *vtmp = new std::vector<mpz_ptr>;
		for (size_t k = 0; k <= t; k++)
		{
			mpz_ptr tmp1 = new mpz_t();
			mpz_init(tmp1);
 			vtmp->push_back(tmp1);
		}
		C_ik.push_back(*vtmp);
	}
	size_t simulate_faulty_randomizer = mpz_wrandom_ui() % 2L;

	try
	{
if (i == 0)
std::cerr << "GENERATE(1)" << std::endl;
		// 1. Each player $P_i$ performs a Pedersen-VSS of a random
		//    value $a_i$:
		// (a) $P_i$ picks $t$-deg. polynomials
		//     $f_{a_i}(z) = \sum_{k=0}^t c_{ik} z^k$,
		//     $f_{\hat{a_i}}(z) = \sum_{k=0}^t \hat{c}_{ik} z^k$
		for (size_t k = 0; k <= t; k++)
		{
			mpz_srandomm(c_ik[k], q);
			mpz_srandomm(hatc_ik[k], q);
		}
		// Let $a_i = f_{a_i}(0)$ and $\hat{a_i} = f_{\hat{a_i}}(0)$.
		// $P_i$ broadcasts $C_{ik} = g^{c_{ik}} h^{\hat{c}_{ik}}$
		// for $k = 0..t$. 
		for (size_t k = 0; k <= t; k++)
		{
			mpz_fspowm(fpowm_table_g, foo, g, c_ik[k], p);
			mpz_fspowm(fpowm_table_h, bar, h, hatc_ik[k], p);
			mpz_mul(C_ik[i][k], foo, bar);
			mpz_mod(C_ik[i][k], C_ik[i][k], p);
			rbc->Broadcast(C_ik[i][k]);
		}
		// Set $F_{a_i}(z) = \prod_{k=0}^t (C_{ik})^{z^k}$.
		// $P_i$ sends to $P_j$ shares $\alpha_{ij} = f_{a_i}(j)$,
		// $\hat{\alpha}_{ij} = f_{\hat{a}_i}(j)$ for each $j = 1..n$.
		for (size_t j = 0; j < n; j++)
		{
			mpz_set_ui(alpha_ij[i][j], 0L);
			mpz_set_ui(hatalpha_ij[i][j], 0L);
			for (size_t k = 0; k <= t; k++)
			{
				mpz_ui_pow_ui(foo, j + 1, k); // adjust index $j$ in computation
				mpz_mul(bar, foo, hatc_ik[k]);
				mpz_mod(bar, bar, q);
				mpz_mul(foo, foo, c_ik[k]);
				mpz_mod(foo, foo, q);
				mpz_add(alpha_ij[i][j], alpha_ij[i][j], foo);
				mpz_mod(alpha_ij[i][j], alpha_ij[i][j], q);				
				mpz_add(hatalpha_ij[i][j], hatalpha_ij[i][j], bar);
				mpz_mod(hatalpha_ij[i][j], hatalpha_ij[i][j], q);
			}
			if (j != i)
			{
				if (simulate_faulty_behaviour)
				{
					mpz_add_ui(alpha_ij[i][j], alpha_ij[i][j], 1L);
				}
				aiou->Send(alpha_ij[i][j], j);
				if (simulate_faulty_behaviour && simulate_faulty_randomizer)
				{
					mpz_add_ui(hatalpha_ij[i][j], hatalpha_ij[i][j], 1L);
				}
				aiou->Send(hatalpha_ij[i][j], j);
			}
		}
if (i == 0)
std::cerr << "GENERATE(2)" << std::endl;
		// (b) Each $P_j$ verifies if
		//     $g^{\alpha_{ij}} h^{\hat{\alpha}_{ij}} = F_{a_i}(j)$
		//     for $i = 1..n$. If the check fails for any $i$, $P_j$
		//     broadcasts a complaint against $P_i$.
		// Note that in this section the indicies $i$ and $j$ are
		// exchanged for convenience.
		for (size_t j = 0; j < n; j++)
		{
			if (j != i)
			{
				for (size_t k = 0; k <= t; k++)
				{
					if (!rbc->DeliverFrom(C_ik[j][k], j))
					{
						err << "P_" << i << ": receiving C_ik failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
						break;
					}
				}
if (i == 0)
std::cerr << "GENERATE(3)" << std::endl;
				if (!aiou->Receive(alpha_ij[j][i], j, aiou->aio_scheduler_direct))
				{
					err << "P_" << i << ": receiving alpha_ij failed; complaint against P_" << j << std::endl;
					complaints.push_back(j);
					continue;
				}
				if (!aiou->Receive(hatalpha_ij[j][i], j, aiou->aio_scheduler_direct))
				{
					err << "P_" << i << ": receiving hatalpha_ij failed; complaint against P_" << j << std::endl;
					complaints.push_back(j);
					continue;
				}
if (i == 0)
std::cerr << "GENERATE(3a)" << std::endl;
			}
		}
if (i == 0)
std::cerr << "GENERATE(4)" << std::endl;
		for (size_t j = 0; j < n; j++)
		{
			// compute LHS for the check
			mpz_fspowm(fpowm_table_g, foo, g, alpha_ij[j][i], p);
			mpz_fspowm(fpowm_table_h, bar, h, hatalpha_ij[j][i], p);
			mpz_mul(lhs, foo, bar);
			mpz_mod(lhs, lhs, p);
			// compute RHS for the check
			mpz_set_ui(rhs, 1L);
			for (size_t k = 0; k <= t; k++)
			{
				mpz_ui_pow_ui(foo, i + 1, k); // adjust index $i$ in computation
				mpz_powm(bar, C_ik[j][k], foo , p);
				mpz_mul(rhs, rhs, bar);
				mpz_mod(rhs, rhs, p);
			}
			// check equation (4)
			if (mpz_cmp(lhs, rhs))
			{
				err << "P_" << i << ": checking 1(b) failed; complaint against P_" << j << std::endl;
				complaints.push_back(j);
			}
		}
		std::sort(complaints.begin(), complaints.end());
		std::vector<size_t>::iterator it =
			std::unique(complaints.begin(), complaints.end());
		complaints.resize(std::distance(complaints.begin(), it));
		for (std::vector<size_t>::iterator it = complaints.begin(); it != complaints.end(); ++it)
		{
			mpz_set_ui(rhs, *it);
			rbc->Broadcast(rhs);
		}
		mpz_set_ui(rhs, n); // send end marker
		rbc->Broadcast(rhs);
if (i == 0)
std::cerr << "GENERATE(5)" << std::endl;
		// (c) If $P_j$ complained against $P_i$, $P_i$ broadcasts
		//     $\alpha_{ij}$, $\hat{\alpha}_{ij}$; everyone verifies
		//     it. If $P_i$ fails this test or receives more than $t$
		//     complaints, exclude $P_i$ from $Qual$.
		complaints.clear();
		for (size_t j = 0; j < n; j++)
			complaints_counter.push_back(0); // initialize counter
		for (size_t j = 0; j < n; j++)
		{
			if (j != i)
			{
				size_t who;
				size_t cnt = 0;
				do
				{
					if (!rbc->DeliverFrom(rhs, j))
					{
						err << "P_" << i << ": receiving who failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
						break;
					}
					who = mpz_get_ui(rhs);
					if (who < n)
					{
						err << "P_" << i << ": receiving complaint against P_" << who << " from P_" << j << std::endl;
						complaints_counter[who]++;
						if (who == i)
							complaints.push_back(j);
					}
					cnt++;
				}
				while ((who < n) && (cnt <= n)); // until end marker received
			}
		}
if (i == 0)
std::cerr << "GENERATE(6)" << std::endl;
		if (complaints_counter[i])
		{
			std::sort(complaints.begin(), complaints.end());
			err << "P_" << i << ": there are " << complaints_counter[i] << " complaints against me from ";
			for (std::vector<size_t>::iterator it = complaints.begin(); it != complaints.end(); ++it)
				err << "P_" << *it << " ";
			err << std::endl;
			for (std::vector<size_t>::iterator it = complaints.begin();
				it != complaints.end(); ++it)
			{
				mpz_set_ui(lhs, *it); // who?
				rbc->Broadcast(lhs);
				rbc->Broadcast(alpha_ij[i][*it]);
				rbc->Broadcast(hatalpha_ij[i][*it]);
			}
		}
		mpz_set_ui(lhs, n); // send end marker
		rbc->Broadcast(lhs);
if (i == 0)
std::cerr << "GENERATE(7)" << std::endl;
		complaints.clear();
		for (size_t j = 0; j < n; j++)
		{
			if (j != i)
			{
				size_t who;
				if (complaints_counter[j] > t)
				{
					complaints.push_back(j);
					continue;
				}
				size_t cnt = 0;
				do
				{
					if (!rbc->DeliverFrom(lhs, j))
					{
						err << "P_" << i << ": receiving who failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
						break;
					}
					who = mpz_get_ui(lhs);
					if (who >= n)
						break; // end marker received
					if (!rbc->DeliverFrom(foo, j))
					{
						err << "P_" << i << ": receiving foo failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
						break;
					}
					if (!rbc->DeliverFrom(bar, j))
					{
						err << "P_" << i << ": receiving bar failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
						break;
					}
					// compute LHS for the check
					mpz_fpowm(fpowm_table_g, foo, g, foo, p);
					mpz_fpowm(fpowm_table_h, bar, h, bar, p);
					mpz_mul(lhs, foo, bar);
					mpz_mod(lhs, lhs, p);
					// compute RHS for the check
					mpz_set_ui(rhs, 1L);
					for (size_t k = 0; k <= t; k++)
					{
						mpz_ui_pow_ui(foo, who + 1, k); // adjust index $j$ in computation
						mpz_powm(bar, C_ik[j][k], foo , p);
						mpz_mul(rhs, rhs, bar);
						mpz_mod(rhs, rhs, p);
					}
					// check equation
					if (mpz_cmp(lhs, rhs))
					{
						err << "P_" << i << ": checking 1(c) failed; complaint against P_" << j << std::endl;
						complaints.push_back(j);
					}
					cnt++;
				}
				while (cnt <= n);
			}
		}
if (i == 0)
std::cerr << "GENERATE(8)" << std::endl;
		for (size_t j = 0; j < n; j++)
			if (std::find(complaints.begin(), complaints.end(), j) == complaints.end())
				Qual.push_back(j);
		err << "P_" << i << ": Qual = { ";
		for (std::vector<size_t>::iterator it = Qual.begin(); it != Qual.end(); ++it)
			err << "P_" << *it << " ";
		err << "}" << std::endl;
		// 2. $P_i$ sets his polynomial share of the generated secret $a$ as
		//    $\alpha_i = \sum_{P_j \in Qual} \alpha_{ji}$, and their
		//    associated randomness as
		//    $\hat{\alpha}_i = \sum_{P_j \in Qual} \hat{\alpha}_{ji}$.
		// Note that in this section the indicies $i$ and $j$ are exchanged
		// again, because the reversed convention is used in section 1(b).
		mpz_set_ui(alpha_i, 0L), mpz_set_ui(hatalpha_i, 0L);
		for (std::vector<size_t>::iterator it = Qual.begin(); it != Qual.end(); ++it)
		{
			mpz_add(alpha_i, alpha_i, alpha_ij[*it][i]);
			mpz_mod(alpha_i, alpha_i, q);
			mpz_add(hatalpha_i, hatalpha_i, hatalpha_ij[*it][i]);
			mpz_mod(hatalpha_i, hatalpha_i, q);
		}
		err << "P_" << i << ": alpha_i = " << alpha_i << std::endl;
		err << "P_" << i << ": hatalpha_i = " << hatalpha_i << std::endl;
		
		if (std::find(Qual.begin(), Qual.end(), i) == Qual.end())
			throw false;
		if (Qual.size() <= t)
			throw false;

		throw true;
	}
	catch (bool return_value)
	{
		// release
		mpz_clear(foo), mpz_clear(bar), mpz_clear(lhs), mpz_clear(rhs);
		for (size_t k = 0; k <= t; k++)
		{
			mpz_clear(c_ik[k]), mpz_clear(hatc_ik[k]);
			delete c_ik[k], delete hatc_ik[k];
		}
		c_ik.clear(), hatc_ik.clear();
		for (size_t j = 0; j < n; j++)
		{
			for (size_t k = 0; k <= t; k++)
			{
				mpz_clear(C_ik[j][k]);
				delete C_ik[j][k];
			}
			C_ik[j].clear();
		}
		C_ik.clear();
		// return
		return return_value;
	}
}

bool JareckiLysyanskayaRVSS::Reconstruct
	(size_t i, std::vector<size_t> &complaints,
	std::vector<mpz_ptr> &a_i_in,
	CachinKursawePetzoldShoupRBC *rbc, std::ostream &err)
{
	// initialize
	mpz_t foo, bar, lhs, rhs;
	mpz_init(foo), mpz_init(bar), mpz_init(lhs), mpz_init(rhs);

	try
	{
		// run reconstruction phase of Pedersen-VSS
		if (complaints.size() > t)
			throw false;
		for (std::vector<size_t>::iterator it = complaints.begin(); it != complaints.end(); ++it)
		{
			// broadcast shares for reconstruction of $z_i$ (where $i = *it$) 
			rbc->Broadcast(alpha_ij[*it][i]);
			// prepare for collecting shares
			std::vector<size_t> parties;
			parties.push_back(i);
			// collect shares $s_{ij}$ from other parties
			for (std::vector<size_t>::iterator jt = Qual.begin(); jt != Qual.end(); ++jt)
			{
				if ((*jt != i) && (std::find(complaints.begin(), complaints.end(), *jt) == complaints.end()))
				{
					if (rbc->DeliverFrom(alpha_ij[*it][*jt], *jt))
						parties.push_back(*jt);
					else
						err << "P_" << i << ": no share from " << *jt << std::endl;					
				}
			}
			// check whether enough shares have been collected
			if (parties.size() <= t)
			{
				err << "P_" << i << ": not enough shares collected" << std::endl;
				throw false;
			}
			if (parties.size() > (t + 1))
				parties.resize(t + 1);
			err << "P_" << i << ": reconstructing parties = ";
			for (std::vector<size_t>::iterator jt = parties.begin(); jt != parties.end(); ++jt)
				err << "P_" << *jt << " ";
			err << std::endl;
			// compute $z_i$ using Lagrange interpolation (without corrupted parties)
			mpz_set_ui(foo, 0L);
			for (std::vector<size_t>::iterator jt = parties.begin(); jt != parties.end(); ++jt)
			{
				mpz_set_ui(rhs, 1L); // compute the optimized Lagrange multipliers
				for (std::vector<size_t>::iterator lt = parties.begin(); lt != parties.end(); ++lt)
				{
					if (*lt != *jt)
						mpz_mul_ui(rhs, rhs, (*lt + 1)); // adjust index in computation
				}
				mpz_set_ui(lhs, 1L);
				for (std::vector<size_t>::iterator lt = parties.begin(); lt != parties.end(); ++lt)
				{
					if (*lt != *jt)
					{
						mpz_set_ui(bar, (*lt + 1)); // adjust index in computation
						mpz_sub_ui(bar, bar, (*jt + 1)); // adjust index in computation
						mpz_mul(lhs, lhs, bar);
					}
				}
				mpz_invert(lhs, lhs, q);
				mpz_mul(rhs, rhs, lhs);
				mpz_mod(rhs, rhs, q);
				mpz_mul(bar, alpha_ij[*it][*jt], rhs);
				mpz_mod(bar, bar, q);
				mpz_add(foo, foo, bar);
				mpz_mod(foo, foo, q);
			}
			mpz_set(a_i_in[*it], foo);
			parties.clear();
		}

		throw true;
	}
	catch (bool return_value)
	{
		// release
		mpz_clear(foo), mpz_clear(bar), mpz_clear(lhs), mpz_clear(rhs);
		// return
		return return_value;
	}
}

JareckiLysyanskayaRVSS::~JareckiLysyanskayaRVSS
	()
{
	mpz_clear(p), mpz_clear(q), mpz_clear(g), mpz_clear(h);
	Qual.clear();
	mpz_clear(alpha_i), mpz_clear(hatalpha_i);
//	for (size_t j = 0; j < y_i.size(); j++)
//	{
//		mpz_clear(y_i[j]);
//		delete y_i[j];
//	}
//	y_i.clear();
//	for (size_t j = 0; j < z_i.size(); j++)
//	{
//		mpz_clear(z_i[j]);
//		delete z_i[j];
//	}
//	z_i.clear();
	for (size_t j = 0; j < alpha_ij.size(); j++)
	{
		for (size_t i = 0; i < alpha_ij[j].size(); i++)
		{
			mpz_clear(alpha_ij[j][i]);
			delete alpha_ij[j][i];
		}
		alpha_ij[j].clear();
	}
	alpha_ij.clear();
	for (size_t j = 0; j < hatalpha_ij.size(); j++)
	{
		for (size_t i = 0; i < hatalpha_ij[j].size(); i++)
		{
			mpz_clear(hatalpha_ij[j][i]);
			delete hatalpha_ij[j][i];
		}
		hatalpha_ij[j].clear();
	}
	hatalpha_ij.clear();

	mpz_fpowm_done(fpowm_table_g), mpz_fpowm_done(fpowm_table_h);
	delete [] fpowm_table_g, delete [] fpowm_table_h;
}

// ===================================================================================================================================


