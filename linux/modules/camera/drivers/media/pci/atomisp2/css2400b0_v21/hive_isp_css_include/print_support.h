/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __PRINT_SUPPORT_H_INCLUDED__
#define __PRINT_SUPPORT_H_INCLUDED__
extern int (*sh_css_printf) (const char *fmt, va_list args);
/* depends on host supplied print function in ia_css_init() */
static __inline void ia_css_print(const char *fmt, ...)
{
	va_list ap;
	if (sh_css_printf) {
		va_start(ap, fmt);
		sh_css_printf(fmt, ap);
		va_end(ap);
	}
}
#endif /* __PRINT_SUPPORT_H_INCLUDED__ */
