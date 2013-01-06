OBS-DatapathPlugin
==================

Datapath Vision series capture plugin for OBS

Licence: GNU GPLv2

Started as an attempt to squeeze more performance out of OBS in conjunction with my capture card, for now this is mostly a proof of concept rather than a full-fledged plugin.

This is a native capture plugin leveraging the RGBEasy API instead of DirectShow. Right now it seems to be marginally faster than OBS' own DirectShow plugin sometimes, and quite a bit slower other times. I am attributing this mostly to the extra memcopy I have to do between the capture driver DMA surface and the Direct3D mapped texture, but there are other strange factors at work on my PC.

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
- Resolution autodetection

Planned features:
- Direct DMA to Direct3D mapped texture (I am having unusual problems getting this working)
- Input port selection
- Cropping
- Automatic resolution switching
- Picture adjustment controls
- Picture adjustment presets
- Colour space selection
- Deinterlacing
- Prescaling (for 15KHz VGA sources)

(Note that I do not work for nor am affiliated with Datapath other than simply being one of their customers. Depending on things, I may never get to implementing certain planned features or I could stop developing it entirely.)