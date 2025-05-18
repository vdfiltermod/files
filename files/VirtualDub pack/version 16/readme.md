### `VirtualDub_FilterMod_39758.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_39758.zip/download`
- Upload date: `2017-08-13 19:54:17 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_39758.zip


### `readme.md`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/readme.md/download`
- Upload date: `2017-08-13 19:51:31 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/readme.md


### `VirtualDub_FilterMod_40087_5.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_40087_5.zip/download`
- Upload date: `2017-07-25 19:57:03 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_40087_5.zip


### `VirtualDub_FilterMod_39859.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_39859.zip/download`
- Upload date: `2017-07-15 10:17:48 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_39859.zip


### `VirtualDub_FilterMod_40035.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_40035.zip/download`
- Upload date: `2017-07-01 14:16:55 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_40035.zip


### `VirtualDub_FilterMod_40087.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_40087.zip/download`
- Upload date: `2017-06-25 11:04:43 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_40087.zip


### `VirtualDub_FilterMod_40121.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2016/VirtualDub_FilterMod_40121.zip/download`
- Upload date: `2017-06-18 18:38:57 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-16/VirtualDub_FilterMod_40121.zip

---

Includes both **x86 and x64 binaries**

Includes plugins:

*  x264 encoder
*  lagarith encoder
*  avlib-1 (collection of various plugins based on FFMPEG)
  > caching input driver (1.17)  
  > video compression to Huffyuv, FFV1, Prores  
  > audio compression to MP3, AAC (**experimental**)  
  > output formats support (**experimental**)  
  > fflayer video filter  
*  MPEG-2 decoder v4.4 by fccHandler
*  master blend
*  gauss blur and unsharp
*  rgb scale
*  rgb levels
*  6-axis color correction
*  AviSynth/VapourSynth script editor

[overview](https://sourceforge.net/p/vdfiltermod/wiki/)
==================

# Version 16:

update 6 (build 40121) 

* fixed 'swap panes' behavior
* caching input driver:
  > fixed seeking with image sequence  
* fflayer filter:
  > fixed alpha to alpha compositing  

update 5 (build 40087) 

* caching input driver:
  > recognize 'drop frames' in AVI  
  > more robust direct stream copy for various intra etc formats:  
  lagarith, ffv1, canopus, dv, ...  
  > fixed several audio bugs  
  > fixed 'frame not found' after saving  

update 4 (build 40087) 

* fixes in command line behavior: #57
* caching input driver: 
  > fixed segment handling in direct copy mode

update 3 (build 40035) 

* fixed navigation during preview (was not complete in previous update) #49 
* apply audio conversion #52
* fixed opening avs #50, #54
* fixed opening asf

update 2 (build 39859) 

* new commands supported during preview (via hotkey):
  > goto prev/next range  
  > goto time/frame 
* caching input driver: 
  > fixed slow seeking with some files  

update 1 (build 39758) 

* changed keyboard seeking behavior, now controlled by menu option (Drop frames when seeking)
* [changes in Open Video dialog](https://sourceforge.net/p/vdfiltermod/wiki/OpenVideo)
* changes in filters list dialog
* fixed Append Video
* fixed copy to clipboard
* fixed bug #46 (timeline corruption)
* caching input driver: 
  > known file extensions now grouped as video, audio, and images  
  > improved memory handling  
  > fixed audio seeking error  
  > fixed handling of GBRP14 pixel format  
  > other fixes  

# Version 15:

update 6 (build 39460) 

* fixed various issues with output formats
  x264 encoded in mp4/mkv should be compatible with more programs now  
* fixed opening multiple video segments and saving as job or project  
* fixed bug: copy frame to clipboard (or save to file) sometimes unavailable  
* improved "save image" and "save project" dialogs
* fixed #42 (output to alternate format not working with jobs)

update 5 (build 39240) 

* x264 encoder now writes correct timestamps to mp4,mov,mkv,...  
* messages from x264 encoder are displayed in main log (F8)  
* fixed various bugs  

update 4 (build 39148) 

* updated cropping/blending dialogs, #30
* added MPEG-2 plugin, disabled internal MPEG-1 decoder
* caching input driver (part of avlib-1.vdplugin): 
  > fixed performance: frame threading was not enabled in many cases  
  > fixed more timing issues, #35

update 3 (build 39005) 

* fixed View->Maximize (odd behavior in some cases)  
* fixed scene navigation (ignored threshold settings)  
* added external encoder formats: yuv 16-bit  
* added lagarith encoder  
* caching input driver (part of avlib-1.vdplugin): 
  > fixed #29 (direct stream copy issue)  

update 2 (build 38919) 

* added View->Maximize (compact title & status bars)
* fixed memory leaks in some internal filters
* fixed crash and wrong format in normal recompress (#27)
* fixed seeking error sometimes with h264,h265 (#27)
* fixed preview when audio fails (#24)
* supply default layout for surround audio from AviSynth
* support IEEE float audio from AviSynth

update 1 (build 38834) 

* improved audio
  > multichannel and float pcm works for some operations  
  > audio display is more reliable now and can show multichannel and float formats  
* caching input driver (part of avlib-1.vdplugin): 
  > now decodes audio to multichannel and float formats (use audio->conversion to downmix)  
  > fixed various bugs  
* bugs
  > preview was not possible in fast recompress  
  > display panes did not update on single-step  
  > display panes did not update after "dubbing operation"  
  > .vdproject was not supported on drag-n-drop  

# Version 14:

See updated page [deep color and alpha](https://sourceforge.net/p/vdfiltermod/wiki/format_changes/)

update 3 (build 38543) 

* fixed broken Pixel Format in scripts (#20)
* fixed choppy/freezing preview in certain conditions
* improved automatic format detection for VFW decoder

update 2 (build 38494) 

* Optimized YUV conversions from/to 8-bit
* Added Tools->Benchmark analyze pass
* Small ui fixes
* Fixed bug: fast recompress swapping UV planes
* Fixed bug: video codec settings not saved
* Fixed bug: cannot encode with yuv420-709 and similar
* Fixed YUV-to-YUV color space conversions with 16-bit formats
* Added Y416 input support
* Fixed crash with P210,P216

update 1 (build 38276) 

* Support input in YUV formats: 444, 422, 420 up to 16 bits per component.  
Possible sources:  
  > ffmpeg (caching input driver): direct support of compatible formats, also allowed conversion to YUV-444-P16 by request.  
  > vfw/avi (v210, P210, P216, ffmpeg fourccs of 422P10, 422P16).  
  > also possible to use Cineform vfw codec with caching input driver in v210 mode.  
* Optimized conversion of 16-bit YUV to 16-bit RGB.  
* Added support for 16-bit formats to internal Resize filter.  
* Fixed incorrect dual pane resizing (#19).  
* Fixed incompatible format selection when exporting images.  
* Enabled alpha in png export.  

* FFMPEG updated to 2016-12-27.  

* caching input driver: 
  > optimized YUV input (see above)  
  > fixed handling of some image sequence formats  
  > added static configure dialog (see options->plugins...)  
  > added decoding of Adobe Photoshop (psd) images  
* Huffyuv and FFV1 encoders:  
  > added YUV options: bit depth 9..16  


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

