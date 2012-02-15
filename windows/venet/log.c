/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 * 
 *  log.c: routines for spewing text to COM1.
 *
 * Author:
 *     Peter W. Morreale <pmorreale@novell.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "vbus.h"
#include "precomp.h"
#include "stdarg.h"
#include "string.h"

#ifdef VENET_DEBUG

void
vlog(const char *fmt, ...)
{
	char 		msg[1024];
	NTSTATUS	rc;
	char		*m;
	char		*label = "venet: ";
    	va_list 	va;


	m = label;
	while(*m) {
       		WRITE_PORT_UCHAR(VENET_DBG_PORT, *m);
		m++;
	}

	*msg = '\0';
	va_start(va, fmt );
    	rc = RtlStringCbVPrintfA(msg, 1024, fmt, va);
	va_end(va);

	m = msg;
	while(*m) {
       		WRITE_PORT_UCHAR(VENET_DBG_PORT, *m);
		m++;
	}

       	WRITE_PORT_UCHAR(VENET_DBG_PORT, '\n');
	return;
}
#endif
