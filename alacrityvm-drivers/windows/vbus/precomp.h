/*
 * precomp.h for vbus driver
 */
#define WIN9X_COMPAT_SPINLOCK
#include <ntddk.h> 
#include <wdf.h>

typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

#include <initguid.h> // required for GUID definitions
#include <wdmguid.h> // required for WMILIB_CONTEXT
#include <wmistr.h>
#include <wmilib.h>
#include <ntintsafe.h>

#pragma warning(disable:4214)
#pragma warning(disable:4201)
#pragma warning(disable:4115)

#include "ntddndis.h" // for OIDs

#pragma warning(default:4214)
#pragma warning(default:4201)
#pragma warning(default:4115)

#include "public.h"
#include "vbus.h"
