/* -*- mode: c++; coding: utf-8-unix -*-
 *
 * Copyright 2013 MTA SZTAKI
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/** \file

    \brief Implementation of the matrix operations specified in rnc-lib/matrix.h
 */

#include <rnc-lib/matrix.h>
#include <rnc-lib/mt.h>
#include <time.h>
#include <string.h>
#include <glib.h>
#include <mkstr>
#include <auto_arr_ptr>
#include <string>

using namespace rnc::fq;

namespace rnc
{
namespace matrix
{

int BLOCK_SIZE = 1;
int NCPUS = 2;

static void checkGError(char const * const context, GError *error)
{
	if (error != 0)
	{
                std::string ex = MKStr() << "glib error: "
				    << context << ": " << error->message;
		g_error_free(error);
		throw ex;
	}
}

void set_identity(Matrix &m) throw()
{
        const int ncols = m.ncols;
        Row *row = m.rows;
	for (int i=m.nrows; i>0; --i, ++row)
        {
                Element *elem = *row;
		for (int j=0; j<ncols; ++j, ++elem)
                        *elem = (int)i==j;
        }
}
/*
void copy(const Matrix m, Matrix md) throw()
{
        const int nrows = m.nrows;
        const int ncols = m.ncols;
        const size_t rowsize = ncols*sizeof(Element);

        Row *row = m.rows, *rowD = md.rows;
	for (int i=0; i<nrows; ++i, ++row, ++rowD)
                memcpy(rowD->elements, row->elements, rowsize);
}

bool invert(const Matrix m_in, Matrix res) throw ()
{
	auto_matr_ptr m_ap = create_matr(m_in.nrows, m_in.ncols);
        Matrix m = m_ap;
	copy(m_in, m, rows, cols);
	set_identity(res, rows, cols);

	for (int i=0; i<rows; ++i)
	{
		fq_t *const m_i=RA(m,i), *const res_i=RA(res,i);

		//normalize row
		const fq_t p = RE(m_i,i);
		if (p == 0) return false;

		for (int c=0; c<cols; ++c)
		{
			if (c>=i) divby(RE(m_i,c), p);
			divby(RE(res_i,c), p);
		}

		for (int r=i+1; r<cols; ++r)
		{
			fq_t *const m_r=RA(m,r), *const res_r=RA(res,r);
			const fq_t h = RE(m_r,i);

			for (int c=0; c<cols; ++c)
			{
				if (c>=i) addto_mul(RE(m_r,c), RE(m_i,c), h);
				addto_mul(res_r[c], RE(res_i,c), h);
			}
		}
	}

	//back-substitution
	for (int i=rows-1; i>=0; --i)
	{
		fq_t *const res_i =RA(res,i);

		for (int r=i-1; r>=0; --r)
		{
			fq_t *const m_r = RA(m,r), *const res_r = RA(res,r);
			const fq_t h = RE(m_r,i);
			RE(m_r,i) = 0;

			for (int c=0; c<cols; ++c)
				addto_mul(RE(res_r,c), RE(res_i,c), h);
		}
	}

	return true;
}

/** \brief Description of a single workunit

    Matrix multiplication threads process workunits described with this
    construct.
 * /
typedef struct muldata
{
        /// \brief Left-hand side matrix
	const Matrix m1;
        /// \brief Right-hand side matrix
	const Matrix m2;
        /// \brief Result address
	Matrix md;
        /// \brief Rows count of #m1
	const int rows1;
        /// \brief Columns of #m1 (== rows of #m2)
	const int cols1;
        /// \brief Columns of #m2
	const int cols2;
} muldata;

void mulrow_blk(gpointer bb, gpointer d)
{
	const muldata *data = reinterpret_cast<muldata*>(d);

	const int b = BLOCK_SIZE;
	const int cols1 = data->cols1;
	const int cols2 = data->cols2;
	const int rows1 = data->rows1;
	Matrix md = data->md;
	const Matrix m1 = data->m1;
	const Matrix m2 = data->m2;

	int k, j;
	const long i = (long)bb - 1;
	const int li=i+b > rows1 ? rows1 : i+b;
	int lk, lj;
	int i0,j0,k0;

	for (j=0, lj=b; j < cols2; j+=b, lj+=b) {
		if (lj > cols2) lj=cols2;

		for (k=0, lk=b; k < cols1; k+=b, lk+=b) {
			if (lk > cols1) lk=cols1;

			for (i0=i; i0<li; ++i0) {
				for (k0=k; k0<lk; ++k0) {
					const fq_t e1 = E_(m1, i0, k0, cols1);

					for (j0=j; j0<lj; ++j0) {
						addto_mul(*A_(md, i0, j0, cols2), e1, E_(m2, k0, j0, cols2));
					}
				}
			}
		}
	}
}

void mulrow_nonblk(gpointer row, gpointer d)
{
	const long i = (long)row-1;
	const muldata *data = reinterpret_cast<muldata*>(d);
	const int cols2=data->cols2;
	const int cols1=data->cols1;
	Matrix md = data->md;
	const Matrix m1 = data->m1;
	const Matrix m2 = data->m2;

	int j, k;

	for (j=0; j<cols2; ++j)
	{
		fq_t s = 0;
		for (k=0; k<cols1; ++k) {
			s = add(s, fq::mul(E_(m1,i,k,cols1),
                                           E_(m2, k,j,cols2)));
		}
		E_(md,i,j,cols2) = s;
	}
}

void pmul_blk(const Matrix m1, const Matrix m2, Matrix md,
	      const int rows1, const int cols1, int const cols2)
{
	struct muldata d = { m1, m2, md, rows1, cols1, cols2 };
	GError *error = 0;

	GThreadPool *pool = g_thread_pool_new(mulrow_blk, &d,
					      NCPUS, true, &error);
	checkGError("g_thread_pool_create", error);

	for (long i=1; i<=rows1; i+=BLOCK_SIZE) {
		g_thread_pool_push(pool, (void*)i, &error);
		checkGError("g_thread_pool_push", error);
	}

	g_thread_pool_free(pool, false, true);
}

void pmul_nonblk(const Matrix m1, const Matrix m2, Matrix md,
	      const int rows1, const int cols1, int const cols2)
{
	struct muldata d = { m1, m2, md, rows1, cols1, cols2 };
	GError *error = 0;

	GThreadPool *pool = g_thread_pool_new(mulrow_nonblk, &d,
					      NCPUS, true, &error);
	checkGError("g_thread_pool_create", error);

	for (long i=1; i<=rows1; ++i) {
		g_thread_pool_push(pool, (void*)i, &error);
		checkGError("g_thread_pool_push", error);
	}

	g_thread_pool_free(pool, false, true);
}

void pmul(const Matrix m1, const Matrix m2, Matrix md,
	 const int rows1, const int cols1, int const cols2)
{
	if (NCPUS == 1)
	{
		mul(m1, m2, md, rows1, cols1, cols2);
	}
	else
	{
		if (BLOCK_SIZE == 1)
			pmul_nonblk(m1, m2, md, rows1, cols1, cols2);
		else
			pmul_blk(m1, m2, md, rows1, cols1, cols2);
	}
}

void mul_nonblk(const Matrix m1, const Matrix m2, Matrix md,
	 const int rows1, const int cols1,int const cols2)
{
	for (int i=0; i<rows1; ++i)
		for (int j=0; j<cols2; ++j)
		{
			fq_t s = 0;
			for (int k=0; k<cols1; ++k)
				addto_mul(s, E_(m1,i,k,cols1), E_(m2,k,j,cols2));
			E_(md,i,j,cols2) = s;
		}

}
void mul_blk(const Matrix m1, const Matrix m2, Matrix md,
	 const int rows1, const int cols1,int const cols2)
{
	int i, j, k, i0,j0,k0, li, lj, lk;

	memset(md, 0, rows1*cols2*sizeof(fq_t));

	for (i=0, li=BLOCK_SIZE; i<rows1; li+=BLOCK_SIZE, i+=BLOCK_SIZE) {
		if (li > rows1) li=rows1;
		for (k=0, lk=BLOCK_SIZE; k<cols1; lk+=BLOCK_SIZE, k+=BLOCK_SIZE) {
			if (lk > cols1) lk=cols1;

			for (j=0, lj=BLOCK_SIZE; j<cols2; lj+=BLOCK_SIZE, j+=BLOCK_SIZE) {
				if (lj > cols2) lj=cols2;

				for (i0=i; i0<li; ++i0) {
					for (k0=k; k0<lk; ++k0) {
						const fq_t e1 =  E_(m1, i0, k0, cols1);
						for (j0=j; j0<lj; ++j0) {
							addto_mul(*A_(md, i0, j0, cols2), e1, E_(m2, k0, j0, cols2));
						}
					}
				}
			}
		}
	}

}

// (rows1 x cols1) * (cols1 x cols2) = (rows1 x cols2)
void mul(const Matrix m1, const Matrix m2, Matrix md,
	 const int rows1, const int cols1,int const cols2)
{
	if (BLOCK_SIZE == 1)
		mul_nonblk(m1, m2, md, rows1, cols1, cols2);
	else
		mul_blk(m1, m2, md, rows1, cols1, cols2);
}

void rand_matr(Matrix m, const int rows, const int cols)
{
        random::mt_state rnd_state;
        random::init(&rnd_state, time(NULL));

        int i, j;
        Row *row;
        Element *elem;
	for (i=0, row=m; i<rows; ++i, ++row)
		for (j=0, elem=*row; j<cols; ++j, ++elem)
                        *elem = random::generate_fq(&rnd_state);
}
*/



}
}
