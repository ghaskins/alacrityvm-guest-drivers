/*****************************************************************************
 *
 *   File Name:      xennet_ver.h
 *
 *   %version:       14 %
 *   %derived_by:    kallan %
 *   %date_modified: Thu Jul 09 11:54:09 2009 %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2008-2009 Unpublished Work of Novell, Inc. All Rights Reserved.
 * 
 * THIS IS AN UNPUBLISHED WORK OF NOVELL, INC.  IT CONTAINS NOVELL'S 
 * CONFIDENTIAL, PROPRIETARY, AND TRADE SECRET INFORMATION.  NOVELL
 * RESTRICTS THIS WORK TO NOVELL EMPLOYEES WHO NEED THE WORK TO PERFORM
 * THEIR ASSIGNMENTS AND TO THIRD PARTIES AUTHORIZED BY NOVELL IN WRITING.
 * THIS WORK MAY NOT BE USED, COPIED, DISTRIBUTED, DISCLOSED, ADAPTED,
 * PERFORMED, DISPLAYED, COLLECTED, COMPILED, OR LINKED WITHOUT NOVELL'S
 * PRIOR WRITTEN CONSENT.  USE OR EXPLOITATION OF THIS WORK WITHOUT
 * AUTHORIZATION COULD SUBJECT THE PERPETRATOR TO CRIMINAL AND  CIVIL
 * LIABILITY.
 *****************************************************************************/

#ifndef _XENNET_VER_H
#define _XENNET_VER_H


#if defined (NDIS60_MINIPORT)
#define VER_FILEVERSION				1,5,0,12
#define VER_FILEVERSION_STR			"1.5.0.12\0"

#define VNIF_MAJOR_DRIVER_VERSION	0x01
#define VNIF_MINOR_DRIVER_VERSION	0x05

#else
#define VER_FILEVERSION             1,5,0,12
#define VER_FILEVERSION_STR         "1.5.0.12\0"

#define VNIF_MAJOR_DRIVER_VERSION	0x01
#define VNIF_MINOR_DRIVER_VERSION	0x05
#endif

#define VNIF_VENDOR_DRIVER_VERSION	((VNIF_MAJOR_DRIVER_VERSION << 16) | \
									VNIF_MINOR_DRIVER_VERSION)

#endif
