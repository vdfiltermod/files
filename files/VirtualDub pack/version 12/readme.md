Includes plugins:

*  caching input driver (1.9)
*  fflayer
*  master blend
*  gauss blur and unsharp
*  rgb scale
*  rgb levels
*  6-axis color correction
*  AviSynth/VapourSynth script editor

[overview](https://sourceforge.net/p/vdfiltermod/wiki/)

**Late changes**

* fixed [bug #7](https://sourceforge.net/p/vdfiltermod/tickets/7/)
* fixed dx9 display bug  
* fixed VDXA (was broken since version 7)  

# Version 12:

* merged apng support (thanks to Max Stepin)
* added file->info dialogs for image formats
* added [alpha display options in panes](https://sourceforge.net/p/vdfiltermod/wiki/format_changes/)
* added "zip" tiff compression
* fixed disappearing File->Export
* fixed memory leak
* caching input driver: 
  > Added file extensions  
  > Fixed XP compatibility  

# Version 11:

* added [script editor](https://sourceforge.net/p/vdfiltermod/wiki/scripted/)

# Version 10:

* Improve display pane resizing (allows zooming and panning)
* Fix color picker readout with yuv formats
* Add preview button to null transform filter
* Cancel self capturing when screen capturing
* Support b64a uncompressed AVI

# Version 9:

* [Improved timeline control](https://sourceforge.net/p/vdfiltermod/wiki/timeline/)
* other fixes

plugin changes

* caching input driver: 
  > Added export->Stream copy utility  
  > Enabled audio stream selection  
  > Fixed bug: pcm audio in avi gets corrupted  

# Version 8:

* Enable UtVideo Pro RGB 10bit (UQRG/UQRA)
* More info in color picker

plugin changes

* Updated FFMPEG to 2016-04-28
* added fflayer
* added 6-axis color correction
* caching input driver: 
  > Enabled internal cineform decoder (use "all formats" dropdown)
* master blend:
  > added rgb waveform display (click on histogram)  
  > other small ui fixes

# Version 7:

*  Fixed crop dialog (was broken in version 6)
*  Optimized yuv->rgb conversion (8 bit, FR mode)
*  Updated rgb64->rgb32 conversion
*  Format conversion filter can run on multiple threads
*  Repaired builtin Profiler

# Version 6:

*  Added Export single image
*  Improved jpeg export
*  Added Filter View
*  Improved Filters dialog
*  Filter windows respond to new hotkeys
*  Improved v210 pixel format support
*  Fixed stuck UI during playback
*  Fixed some glitches with preview windows

[more details](https://sourceforge.net/p/vdfiltermod/wiki/changes6/)

plugin changes

* caching input driver: fix flipped image when decoding CFHD to rgb32

# Version 5:

*  updated export using external encoder

