modifying playerprefs using monostring and dobbyhook, 

Hooking playerprefs for specific ones since modifying it would globally fuck everything up

ripping/finding playerpref names like "CheaterDetect" or "Cheats12" to set them all to 0, useful, add your own filters too


using polarmods gui easier to import 

const-string v0, "native-lib"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

for unityplayeractivity

must enable file permission to generate the txt within injected application
