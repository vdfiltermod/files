### `VirtualDub_pack_38012.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub_pack_38012.zip/download`
- Upload date: `2016-11-06 22:23:19 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub_pack_38012.zip


### `readme.md`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/readme.md/download`
- Upload date: `2016-11-06 22:07:51 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/readme.md


### `VirtualDub_pack_38098.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub_pack_38098.zip/download`
- Upload date: `2016-11-06 22:05:14 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub_pack_38098.zip


### `VirtualDub64_pack_38099.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub64_pack_38099.zip/download`
- Upload date: `2016-10-25 20:54:31 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub64_pack_38099.zip


### `VirtualDub64_pack_38013.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub64_pack_38013.zip/download`
- Upload date: `2016-10-25 20:54:15 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub64_pack_38013.zip


### `VirtualDub_pack_38042.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub_pack_38042.zip/download`
- Upload date: `2016-10-21 08:17:46 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub_pack_38042.zip


### `VirtualDub64_pack_38043.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2013/VirtualDub64_pack_38043.zip/download`
- Upload date: `2016-10-21 08:17:30 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-13/VirtualDub64_pack_38043.zip

---

Includes plugins:

*  x264 encoder
*  Huffyuv encoder (FFMPEG version)
*  FFV1 encoder
*  caching input driver (1.11)
*  fflayer
*  master blend
*  gauss blur and unsharp
*  rgb scale
*  rgb levels
*  6-axis color correction
*  AviSynth/VapourSynth script editor

[overview](https://sourceforge.net/p/vdfiltermod/wiki/)


# Version 13:

Major changes in this version:  
Added [integrated encoders](https://sourceforge.net/p/vdfiltermod/wiki/compression/) for H264, Huffyuv, FFV1.  
Enabled direct copy and smart rendering modes with caching input driver [(see note)](https://sourceforge.net/p/vdfiltermod/wiki/direct_copy/).  
Improved and added new UI for dialogs: Save AVI, compression, filter cropping, filter blending.  
Filter effect can be restricted to rectangle (useful for comparing).  

update 9 (build 38098)  
bugfix  
polishing to Export->Stream Copy: show progress, activate by hotkey, fix timestamp bias  

update 8 (build 38043)  
bugfix (#17, errors with input display pane)  
added alpha support for MagicYUV codec  

update 7 (build 38012)  
bugfix (#15)  
added command "Edit->Markers from keys"  

update 6 (2016-10-14)  
bugfix (input driver #13, #14)  

update 5 (build 38004)  
bugfix  

update 4 (build 37988)

* updated FFMPEG to 2016-09-22
* added rgb 16 bit to FFV1
* fixed compatibility with MPEG2 input driver
* caching input driver: 
  > added support for direct copy and smart rendering (limited to I-frame formats)  
  > improved performance with rgb formats  

update 3 (build 37959)

* added FFV1 encoder (yuv 8, rgb 8-14)
* added Huffyuv encoder (yuv 8, rgb 8-14)
* added UI to check and change compression before saving avi
* fixed garbage alpha when writing rgba
* other fixes
* caching input driver: 
  > added GoPro info

update 2 (build 37812)

* fixed [bug #8](https://sourceforge.net/p/vdfiltermod/tickets/8/)
* fixed Filter View command
* fixed many other issues
* cropping dialog moved to generic preview
* added crop filter to replace null transform for cropping
* fill filter moved to generic preview and repaired
* added blending dialog

update 1 (build 37579)

* added support for encoder plugin
* changed [compression ui](https://sourceforge.net/p/vdfiltermod/wiki/compression/)
* caching input driver: 
  > Fixed conversion to rgb  
  > Fixed other small bugs  

# Version 12:

* merged apng support (thanks to Max Stepin)
* added file->info dialogs for image formats
* added [alpha display options in panes](https://sourceforge.net/p/vdfiltermod/wiki/format_changes/)
* added "zip" tiff compression
* fixed disappearing File->Export
* fixed memory leak
* fixed [bug #7](https://sourceforge.net/p/vdfiltermod/tickets/7/)
* fixed dx9 display bug  
* fixed VDXA (was broken since version 7)  
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

