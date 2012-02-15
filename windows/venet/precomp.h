
/*
 * Copyright 2009 Novell Inc.  All Rights Reserved.
 *
 *  precomp.h:  Precompilation header.
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

/* Ignore various warnings */

#if 0
#include <initguid.h> 
#include <wmistr.h>
#include <wmilib.h>
#include <ntintsafe.h>
#endif


#pragma warning(disable:4214)   // bit field types other than int
#pragma warning(disable:4200)   // non-standard extension used
#pragma warning(disable:4201)   // nameless struct/union
#pragma warning(disable:4115)   // named type definition in parentheses
#pragma warning(disable:4054)   // cast of function pointer to PVOID


#include <ndis.h>
#include <wdf.h>
#include <ntddndis.h> 
#include <wdmguid.h> 
#include <WdfMiniport.h>
#include <wdftimer.h>

#include <vbus_if.h>
#include <public.h>
#include "venet.h"
