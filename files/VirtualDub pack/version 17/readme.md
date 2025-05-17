### `VirtualDub_FilterMod_40501.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40501.zip/download`
- Upload date: `2017-11-26 17:20:42 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40501.zip


### `VirtualDub_FilterMod_40412.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40412.zip/download`
- Upload date: `2017-11-26 17:19:54 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40412.zip


### `VirtualDub_FilterMod_40538.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40538.zip/download`
- Upload date: `2017-11-13 19:26:59 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40538.zip


### `VirtualDub_FilterMod_40639.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40639.zip/download`
- Upload date: `2017-11-06 22:49:49 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40639.zip


### `VirtualDub_FilterMod_40579.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40579.zip/download`
- Upload date: `2017-11-01 09:25:26 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40579.zip


### `VirtualDub_FilterMod_40301.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40301.zip/download`
- Upload date: `2017-10-24 22:05:46 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40301.zip


### `VirtualDub_FilterMod_40463.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2017/VirtualDub_FilterMod_40463.zip/download`
- Upload date: `2017-09-17 19:57:54 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-17/VirtualDub_FilterMod_40463.zip

---

#[Overview](https://sourceforge.net/p/vdfiltermod/wiki/)

Includes both **x86 and x64 binaries**  
Includes plugins:

*  x264 encoder
*  lagarith encoder
*  avlib-1 (collection of various plugins based on FFMPEG)
  > caching input driver (2.1)  
  > video compression to Huffyuv, FFV1, Prores, VP8, H265, CineForm  
  > audio compression to MP3, AAC  
  > output formats support  
  > fflayer video filter  
*  MPEG-2 decoder v4.5 by fccHandler
*  master blend
*  gauss blur and unsharp
*  rgb scale
*  rgb levels
*  6-axis color correction
*  AviSynth/VapourSynth script editor

-------

# Version 17:

update 8 (build 40639) 

* fixed various colorspace issues
* updated "convert format" filter
* enabled Active Metadata in CineForm
* fixed #71, #69, #68

update 7 (build 40579) 

* replaced MPEG-2 plugin with v4.5 (thank isidroco for noticing)
* fixed possible deadlock when stopping preview
* fixed errors with some odd image sizes
* improved realtime profiler

update 6 (build 40538) 

* added builtin CineForm support [see notes](https://sourceforge.net/p/vdfiltermod/wiki/cineform)
* fixed crash in resize filter

update 5 (build 40501) 

* fixed #66 (crash with CFHD)
* minor fixes

update 4 (build 40463) 

* updated x264 encoder (libx264 152)
* added "ignore index" AVI option
* minor fixes

updates 2..3 (build 40412) 

* fixed capture emulation
* improved realtime profiler

* caching input driver:
  > added options to FFV1 encoder  
  > fixed DPX sequence bug  
  > fixed other bugs

update 1 (build 40301) 

* new supported formats: P010, P016 (input only) 
* new supported formats: v410, Y410, r210, R10k (input/output)  
* new internal filter: DrawText  
* updated tools->test video  
* fixed preview inaccuracy  
* fixed livelock bug  

* FFMPEG updated to 2017-08-27.  

* caching input driver:
  > added options to control memory use  
  > fixed caching strategy, needs less memory now  
  > added simple x265 encoder  
  > added simple VP8 encoder  
  > updated prores encoder  

* updated "master blend" filter  

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

#[Longer history](https://sourceforge.net/p/vdfiltermod/wiki/changes/)
