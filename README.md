# ETDReaderTest
Some test code for Elan's Touchpad Driver. This code isn't production ready, just here as documentation.

I noticed there was no documentation or any info about Elan Touchpad Driver (ETD.sys) so I reverse engineered the interesting parts of the driver.

This is great for ThinkPads (ThinkPad 13, T440, ...). Also tested on Acer and Asus laptops from 2012-2015.

This allows you to read raw input data from Elan hardware if you have Elan's first party driver installed. It's possible that you have a touchpad made by Elan, but your manufacturer like Asus or MSI might have given you their own driver or be using the built in Windows driver. It's possible to install ETDWare from a driver online for these, otherwise you'd have to reverse engineer that manufacturer's or Microsoft's driver to get raw input data.

This raw input data includes:

- Raw touchpad finger positions
    - Older touchpads can detect 3 fingers, and report positions of 2 fingers at most. When 3 fingers are detected, the touchpad will report the average position between the 3.
    - Newer touchpads support 5 fingers.
    - Precision touchpads also report the index of each finger, so when a finger got lifted you know which one.
- Raw pressure (detailed size) data
    - Older touchpads report more precise increments when pressure increases, but less precise when lifted. These also only report one, the same shared value for 1 and 2 fingers, don't support more than 2 as stated in position data.
    - Newer touchpads have equally precise data on press and lift.
    - Precision touchpads (and maybe some older 5 finger touchpads) support pressure for each finger individually.
    - Pressure data is calculated by the touchpad based on the size of the touched area, the more touch area the higher the pressure.
- Raw touch dimension/size data
    - Older touchpads that support 2 finger positions max don't give separate values for X and Y, they also don't report the size for each finger individually, but the sum of all fingers, just like pressure data.
    - Some newer touchpads that support 5 fingers do that too.
    - Precision touchpads report the size for each finger individually in both X and Y direction. These can tell when a finger is rotated and tell the difference between the tip of a finger and the entire length of the finger being placed with their orientation. You can't differentiate between the finger being rotated in quadrants, the data is the same for 90 degrees, 180 and 270 degrees offset.
- ThinkPad's TrackPoint data and TrackPoint related button input:
    - ThinkPad's precision touchpads made by Elan, for example on ThinkPad 13, are ones that send 32 packets per event.
    - You can differentiate between TrackPoint button clicks and ClickPad clicks by the first packet's value's 6th bit. For example, if the touchpad was clicked or touchpad finger event the first packet's value might be 0x5D (0101 1011), and if the left mouse above the ClickPad is clicked, this is 0x5F (0101 1111), so to check if input was TrackPoint input you can do something like ((packets[0] & 0x4) >> 2).
    - TrackPoint events are 7 packets long.
    - You can disable the TrackPoint in ELAN TrackPoint Settings, or make your own implementation to disable the trackpoint in your own app, then you can read the raw TrackPoint data and TrackPoint button presses without it moving the mouse and stuff. You could 3D Print accessories, take off the TrackPoint cap and place it on there to turn the TrackPoint into a joystick, accelerometer, wind meter, scale, whatever you want, though TrackPoints aren't super accurate, you can have some fun with it.

Not all protocols are implemented yet. As far as I can tell there are 2 more protocols. If it doesn't work for your touchpad it shoud normally say your protocol isn't implemented yet, you can just observe how the packet values change as you move your finger over the touchpad to find which packet reports what and implement it yourself.

You can differentiate between an SMBus and PS/2 touchpad by the packet count like Elan's own stuff does or some other way. I'm pretty sure all SMBus touchpads send 32 packets, and TrackPoint input sends 7 packets.

ClickPad is a trackpad that you can press down on to click, like a diving board trackpad. SmartPad is a trackpad that you can't press down on, but these have 2 buttons under the touchpad. ThinkPads have a ClickPad with 3 buttons above it, these 3 buttons are part of the TrackPoint, and despite them not being connected to the computer via the TrackPoint, these buttons only work when the TrackPoint itself is connected.

Important to know, the current implementation just takes the first x packets, there is a 1024 packet buffer, if there is too much lag so that there were more input events before the program awaited the next input event, probably also if there were multiple inputs simultaneously, there will be more packets waiting. The first packet is the oldest packet, for the newest packet you'd have to start with packet 0, read the packet count, decode the data for that group of packets, if the total event packet size is higher than the amount of packets mentioned in the first packets, you need to check the amount of packets in the next packet, so if there were 6 packets but are 12 packets in total you'd need to read the packet count of packet 7 after, then process that one. You must expect it to be possible there are mixes of packet groups of different sizes, like in the 1024 buffer there can be 32 packets, then 7 packets, then 32 packets, then 7 packets, then 7 packets, with the last one being the most recently input one. For accurate tracking, like moving a cursor, you have to go through each packet and take each into account. If your purpose isn't getting the accumulation of input events, like you want to visualise the input in realtime for making a pressure sensitive button or just a touchpad visualiser, you can get away with skipping processing of the first packets and just process the last packet to have the lowest latency possible.

Requires admin rights to wait for kernel data events with newer driver versions. It might close without warning or anything if not run as admin. It's possible to open the driver without admin rights by changing the ACLs, feel free to take the decompiled reference code and implement it if you want:

```cpp
_BOOL8 GetEventPermission()
{
  BOOL v0; // ebx
  DWORD v1; // eax
  HANDLE CurrentProcess; // rax
  HANDLE v3; // rsi
  HANDLE v4; // rsi
  PACL NewAcl; // [rsp+60h] [rbp-A0h] BYREF
  HANDLE TokenHandle; // [rsp+68h] [rbp-98h] BYREF
  PSID psidOwner; // [rsp+70h] [rbp-90h] BYREF
  PSID pSid; // [rsp+78h] [rbp-88h] BYREF
  struct _LUID Luid; // [rsp+80h] [rbp-80h] BYREF
  struct _EXPLICIT_ACCESS_A pListOfExplicitEntries[2]; // [rsp+90h] [rbp-70h] BYREF
  struct _SID_IDENTIFIER_AUTHORITY v12; // [rsp+F0h] [rbp-10h] BYREF
  struct _SID_IDENTIFIER_AUTHORITY pIdentifierAuthority; // [rsp+F8h] [rbp-8h] BYREF

  v0 = 0;
  TokenHandle = 0i64;
  psidOwner = 0i64;
  pSid = 0i64;
  NewAcl = 0i64;
  *(_DWORD *)pIdentifierAuthority.Value = 0;
  *(_WORD *)&pIdentifierAuthority.Value[4] = 256;
  *(_DWORD *)v12.Value = 0;
  *(_WORD *)&v12.Value[4] = 1280;
  if ( AllocateAndInitializeSid(&pIdentifierAuthority, 1u, 0, 0, 0, 0, 0, 0, 0, 0, &pSid) )
  {
    if ( AllocateAndInitializeSid(&v12, 2u, 0x20u, 0x221u, 0, 0, 0, 0, 0, 0, &psidOwner) )
    {
      memset(pListOfExplicitEntries, 0, sizeof(pListOfExplicitEntries));
      pListOfExplicitEntries[0].Trustee.ptstrName = (LPCH)pSid;
      pListOfExplicitEntries[0].grfAccessPermissions = 0x10000000;
      pListOfExplicitEntries[1].Trustee.ptstrName = (LPCH)psidOwner;
      *(_QWORD *)&pListOfExplicitEntries[0].grfAccessMode = 2i64;
      pListOfExplicitEntries[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
      pListOfExplicitEntries[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
      pListOfExplicitEntries[1].grfAccessPermissions = 0x10000000;
      *(_QWORD *)&pListOfExplicitEntries[1].grfAccessMode = 2i64;
      pListOfExplicitEntries[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
      pListOfExplicitEntries[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
      if ( !SetEntriesInAclA(2u, pListOfExplicitEntries, 0i64, &NewAcl) )
      {
        v1 = SetNamedSecurityInfoA(
               (LPSTR)"Global\\ETDOther_GetKernelData",
               SE_KERNEL_OBJECT,
               4u,
               0i64,
               0i64,
               NewAcl,
               0i64);
        if ( v1 )
        {
          if ( v1 == 5 )
          {
            CurrentProcess = GetCurrentProcess();
            if ( OpenProcessToken(CurrentProcess, 0x20u, &TokenHandle) )
            {
              v3 = TokenHandle;
              if ( LookupPrivilegeValueA(0i64, "SeTakeOwnershipPrivilege", &Luid) )
              {
                if ( (unsigned int)sub_7FF758B2C940(v3, 1i64, &Luid) )
                {
                  if ( !SetNamedSecurityInfoA(
                          (LPSTR)"Global\\ETDOther_GetKernelData",
                          SE_KERNEL_OBJECT,
                          1u,
                          psidOwner,
                          0i64,
                          0i64,
                          0i64) )
                  {
                    v4 = TokenHandle;
                    if ( LookupPrivilegeValueA(0i64, "SeTakeOwnershipPrivilege", &Luid) )
                    {
                      if ( (unsigned int)sub_7FF758B2C940(v4, 0i64, &Luid) )
                        v0 = SetNamedSecurityInfoA(
                               (LPSTR)"Global\\ETDOther_GetKernelData",
                               SE_FILE_OBJECT,
                               4u,
                               0i64,
                               0i64,
                               NewAcl,
                               0i64) == 0;
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          v0 = 1;
        }
      }
    }
  }
  if ( psidOwner )
    FreeSid(psidOwner);
  if ( pSid )
    FreeSid(pSid);
  if ( NewAcl )
    LocalFree(NewAcl);
  if ( TokenHandle )
    CloseHandle(TokenHandle);
  return v0;
}
```


When I have time and feel like it, I'll make a C++/CLI library and also a little app that visualises the input data.