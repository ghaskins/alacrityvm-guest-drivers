/*****************************************************************************
 *
 *   File Name:      guid.h
 *
 *   %version:       3 %
 *   %derived_by:    %
 *   %date_modified: %
 *
 *****************************************************************************/
/*****************************************************************************
 * Copyright (C) 2006-2007 Unpublished Work of Novell, Inc. All Rights Reserved.
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

#ifndef _GUID_H
#define _GUID_H

// {1133836d-e283-487e-8d6a-2ad3e157439f}
DEFINE_GUID(GUID_DEVINTERFACE_XENBUS,
            0x1133836d,0xe283,0x487e,0x8d,0x6a,0x21,0xd3,0xe1,0x57,0x43,0x9f);

// {449c996d-751d-4ec2-b998-504e559b13f8}
DEFINE_GUID(GUID_SD_XENBUS_PDO,
            0x449c996d,0x751d,0x4ec2,0xb9,0x98,0x50,0x4e,0x55,0x9b,0x13,0xf8);

// {48b81a04-e879-4486-ae88-019845c1114f}
DEFINE_GUID(GUID_DEVCLASS_XENBUS,
            0x48b81a04,0xe879,0x4486,0xae,0x88,0x01,0x98,0x45,0xc1,0x11,0x4f);

#endif