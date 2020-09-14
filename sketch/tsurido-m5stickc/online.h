/*
 * online.h
 *
 * Online - A online statisics library for Arduino.
 * Author: Hideto Manjo
 * Copyright (c) Hideto Manjo
 *
 * This library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser
 * General Public License as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser
 * General Public License along with this library; if not,
 * write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _ONLINE_H_
#define _ONLINE_H_

#include <math.h>

#ifndef ONLINE_BUFFER_SIZE
#define ONLINE_BUFFER_SIZE 200
#endif

class Online
{
private:
	int _K = 0;
	int _n = 0;
	double _Ex = 0;
	double _Ex2 = 0;
	int _pos = 0;
	int _data[ONLINE_BUFFER_SIZE] = {};

	void add_variable(int* x);
	void remove_variable(int* x);
public:
	Online(/* args */);
	~Online();

	double get_mean(void);
	double get_variance(void);
	void get_stat(int* x, double* mean, double* std);
};

extern Online OL;

#endif
