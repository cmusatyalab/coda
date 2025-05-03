# Example Configuration Files

## /vice/db/user.coda

    root:8eyUkE/DLFLOA:0:1:Local System Manager:/:/bin/csh
    admin:A4SCSN80lqBao:2127:21:AccountsAdministrator:/usr0/admin:/etc/adm
    jjk:trFEGNo9DEuN.:2954:32767:James Kistler:/usr0/jjk:/usr/cs/bin/csh
    satya:.Z.COf9BKQBPM:122:32767:M Satya:/cmu/itc/satya:/bin/csh

## /vice/db/groups.coda

    System:Administrators   -204    dcs jjk mre pkumar satya
    System:coda             -221    dcs jjk mre pkumar satya
    System:codastudents     -222    dcs jjk mre pkumar

## /vice/db/servers

    MAHLER.CODA.CS.CMU.EDU      201
    VIVALDI.CODA.CS.CMU.EDU     202
    RAVEL.CODA.CS.CMU.EDU       203

!!! important
    - The server ids must not be reassigned.
    - Server id's must not be greater than 255.
    - Avoid numbers 0 and 127, which are internally used to identify 'no
      volume' and 'replicated volume'.
    - Aliasing no longer allowed (i.e. do not map more than one name to one
      number).
