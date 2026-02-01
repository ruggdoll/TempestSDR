/*******************************************************************************
 * Copyright (c) 2014 Martin Marinov.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Public License v3.0
 * which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/gpl.html
 *
 * Contributors:
 *     Martin Marinov - initial API and implementation
 ******************************************************************************/

#include <math.h>

#define GAUSSIAN_ALPHA (1.0f)

// N is the number of points, i is between -(N-1)/2 and (N-1)/2 inclusive
#define CALC_GAUSSCOEFF(N,i) expf(-2.0f*GAUSSIAN_ALPHA*GAUSSIAN_ALPHA*(i)*(i)/((N)*(N)))

// Pre-computed Gaussian coefficients for N=5
#define GAUSS_RAW_C2  0.726149f  // exp(-2*1*4/25) = exp(-0.32)
#define GAUSS_RAW_C1  0.923116f  // exp(-2*1*1/25) = exp(-0.08)
#define GAUSS_RAW_C0  1.000000f  // exp(0)
#define GAUSS_NORM    (GAUSS_RAW_C2 + GAUSS_RAW_C1 + GAUSS_RAW_C0 + GAUSS_RAW_C1 + GAUSS_RAW_C2)

static const float c_2 = GAUSS_RAW_C2 / GAUSS_NORM;
static const float c_1 = GAUSS_RAW_C1 / GAUSS_NORM;
static const float c0  = GAUSS_RAW_C0 / GAUSS_NORM;
static const float c1  = GAUSS_RAW_C1 / GAUSS_NORM;
static const float c2  = GAUSS_RAW_C2 / GAUSS_NORM;

void gaussianblur(float * data, int size) {

	float p_2, p_1, p0, p1, p2, data_2, data_3, data_4;
	if (size < 5) {
		p_2 = data[0];
		p_1 = data[1 % size];
		p0 = data[2 % size];
		p1 = data[3 % size];
		p2 = data[4 % size];
	} else {
		p_2 = data[0];
		p_1 = data[1];
		p0 = data[2];
		p1 = data[3];
		p2 = data[4];
	}

	data_2 = p0;
	data_3 = p1;
	data_4 = p2;

	int i;
	const int sizem2 = size - 2;
	const int sizem5 = size - 5;
	for (i = 0; i < size; i++) {

		const int idtoupdate = (i < sizem2) ? (i + 2) : (i - sizem2);
		const int nexti = (i < sizem5) ? (i + 5) : (i - sizem5);

		data[idtoupdate] = p_2 * c_2 + p_1 * c_1 + p0 * c0 + p1 * c1 + p2 * c2;
		p_2 = p_1;
		p_1 = p0;
		p0 = p1;
		p1 = p2;

		if (nexti < 2 || nexti >= 5)
			p2 = data[nexti];
		else {
			switch (nexti) {
			case 2:
				p2 = data_2;
				break;
			case 3:
				p2 = data_3;
				break;
			case 4:
				p2 = data_4;
				break;
			}
		}
	}
}

