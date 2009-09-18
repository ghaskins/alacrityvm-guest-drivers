SUSE Linux Enterprise Virtual Machine Driver Pack
Copyright (C) 2007-2009 Novell, Inc. All rights reserved.

This setup will install the SUSE Linux Enterprise Virtual Machine Driver Pack
on Windows 2000, Windows XP, Windows Server 2003, Windows Vista, and
Windows Server 2008.

Installation:
1. Download the VMDP-WIN-VERSION.exe to your Windows VM.

2. Run VMDP-WIN-VERSION.exe to extract the installation program and support
files into a subfolder of the folder containing VMDP-WIN-VERSION.exe.

3. Run setup.exe from the root of the extracted subfolder and follow the
on-screen instructions.

4. Any "Found New Hardware Wizard" dialogs that pop up after the install can
be ignored or canceled.

To help with automated installations, setup.exe currently has the following
parameters:
/auto_reboot
/no_reboot
/eula_accepted

/auto_reboot will cause the Windows VM to automatically reboot after the setup
has completed.

/no_reboot will prevent the reboot prompt from being displayed after the setup
has completed.

/eula_accepted implies that the EULA has been read, accepted, and agreed to in
all aspects.  Accepting the EULA in this manner will therefore prevent the
EULA acceptance dialog from being displayed during the setup process.

Update:
Setup.exe can be used to update most existing  installations.  To update an
existing installation do the following:
1. Download the VMDP-WIN-VERSION.exe to your Windows VM.

2. Run VMDP-WIN-VERSION.exe to extract the installation program and support
files into a subfolder of the folder containing VMDP-WIN-VERSION.exe.

3. Run setup.exe from the root of the extracted subfolder and follow the
on-screen instructions.

4. Any "Found New Hardware Wizard" dialogs that pop up after the update can
be ignored or canceled.

With some older installations, setup will not be able to perform an update.
In these cases, setup will inform you that the existing installation will
need to be uninstalled first.  Uninstall the existing installation.  Once
completely uninstalled, setup.exe can then be used to install the new files.

Uninstall:
To uninstall the SUSE Linux Enterprise Virtual Machine Driver Pack, execute
the uninstall.exe from the \Program Files\Novell\XenDrv folder.  Uninstall.exe
will uninstall the SUSE Linux Enterprise Virtual Machine Driver Pack and
remove the associated files.

To help with automated uninstalls, uninstall.exe currently has the following
parameters:
/auto_reboot

/auto_reboot will cause the Windows VM to automatically reboot at the
completion of the uninstall.

Setup/Update Cleanup:
Once the SUSE Linux Enterprise Virtual Machine Driver Pack has been completely
installed, the needed files and drivers have been copied to their runtime
locations.  The self-extracting exe, VMDP-WIN-VERSION.exe, and all extracted
files can safely be removed from the system.
