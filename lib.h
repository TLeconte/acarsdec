/*
 *  Copyright (c) 2015 Thierry Leconte
 *
 *
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef lib_h
#define lib_h

unsigned int find_centerfreq(unsigned int minFc, unsigned int maxFc, unsigned int multiplier);
int channels_init_sdr(unsigned int Fc, unsigned int multiplier, float scale);
void channels_mix_phasors(const float complex *phasors, unsigned int len, const unsigned int multiplier);

#endif /* lib_h */
