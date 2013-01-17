OBS-DatapathPlugin
==================

Datapath Vision series capture plugin for OBS

Licence: GNU GPLv2

This is a native capture plugin leveraging the RGBEasy API instead of DirectShow. When using hardware accelerated capture, it captures quite a bit faster than OBS' native Device Source, and uses no extra CPU. Additionally, it offers many handy features unique to the RGBEasy API, like automatically changing the capture resolution when the input resolution changes.

The plugin is heavily based off of Jim's DirectShow plugin (as it was the only plugin available to me at the time). Thanks, Jim!

It has so far only been tested with the VisionRGB-E2s, but it should be compatible with the following cards:

Datapath VisionAV / EMS XtremeRGB-Hdv2  
Datapath VisionDVI-DL / EMS XtremeDVI-DualLink  
Datapath VisionRGB-E1s / EMS XtremeRGB-Ex1  
Datapath VisionRGB-E2s / EMS XtremeRGB-Ex2  
Datapath VisionRGB-PRO2 / EMS PhynxRGB  
Datapath VisionRGB-X2 / EMS XtremeRGB-II  
Datapath VisionSD4+1S / EMS XtremeRGB-Ex4+  
Datapath VisionSD8 / EMS XtremeRGB-Ex8  
Datapath VisionSDI2 / EMS XtremeSDI-2  

Implemented features:
- Video capture (yay!)
- Frame dropping
- Direct DMA to Direct3D mapped texture
- Resolution autodetection
- Automatic resolution switching on video mode change
- Cropping (real-time updated)
- Input port selection (also real-time)
- No signal / signal out of range notices (can be user-defined)

Planned features:
- Flip/rotate
- Picture adjustment controls
- Picture adjustment presets
- Colour space selection
- Deinterlacing
- Prescaling (for 15KHz VGA sources)

Changelog:
20130114
- finally fix DMA capture!
   - should be configurable but is broken so hardcoded for now

20130113
- implement automatic resolution (now default)
- implement no signal / invalid signal images:
   - default ones in OBS\plugins\DatapathPlugin
   - user-defined ones in %APPDATA%\OBS\pluginData\DatapathPlugin
- fix stupidity

20130110
- update crop settings in real-time
- update input selection and detected mode in real-time
- implement mode change callback
- add red/green/blue gain controls to GUI (unused)
- add per-mode settings retention checkbox to GUI (unused)

20130108
- implement cropping
- swap top/left with left/top in config dialogue
- implement automatic bitmap/texture resize
- implement input selection instead of hardcoding to 0
- fix memory leaks
- add comments
- clean up old cruft

(Note that I do not work for nor am affiliated with Datapath other than simply being one of their customers. Depending on things, I may never get to implementing certain planned features or I could stop developing it entirely.)