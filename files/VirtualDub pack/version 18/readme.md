### `VirtualDub_FilterMod_40879.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/VirtualDub_FilterMod_40879.zip/download`
- Upload date: `2018-02-15 10:01:19 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/VirtualDub_FilterMod_40879.zip


### `VirtualDub_FilterMod_41106.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/VirtualDub_FilterMod_41106.zip/download`
- Upload date: `2018-02-15 09:59:52 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/VirtualDub_FilterMod_41106.zip


### `readme.md`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/readme.md/download`
- Upload date: `2018-02-03 12:31:36 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/readme.md


### `VirtualDub_FilterMod_41093.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/VirtualDub_FilterMod_41093.zip/download`
- Upload date: `2018-01-15 22:18:52 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/VirtualDub_FilterMod_41093.zip


### `VirtualDub_FilterMod_40898.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/VirtualDub_FilterMod_40898.zip/download`
- Upload date: `2018-01-13 14:32:30 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/VirtualDub_FilterMod_40898.zip


### `VirtualDub_FilterMod_40716.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2018/VirtualDub_FilterMod_40716.zip/download`
- Upload date: `2017-12-03 20:40:16 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-18/VirtualDub_FilterMod_40716.zip

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

# Version 18:

update 6 (build 41106)

* fixed possible crash with capture (#86)

update 5 (build 41093)

* Small usability fixes:  
  > display option "free adjust" changes scale  
  > navigating slow source with mouse improved  
* Script editor (scripted.vdplugin):  
  > removed loading avisynth.dll at startup  
  > changed "insert trim" etc commands  
  > added "wrap lines" option  

update 4 (build 40898)

* fixed crash with images 
* cosmetic changes in fflayer filter 
* more fixes  

update 2 (build 40879)

* Capture mode changes:
  > list more audio devices (helps with Magewell)  
  > fixed P210 support 

* polished support for formats: P010, P210, P016, P216 (input/output)  
* fixed multiple format conversion issues  
* updated alias format filter (added alpha options)  

* other fixes:
  > garbage in external encoder mode  
  > crash in fast recompress  


update 1 (build 40716)

* Capture mode changes:
  > can select any video codec / any output format  
  > audio can have up to 8 channels  
  > "channel mask" option for audio  
  > panel shows important processing in effect  
  > cleaned some menus  
  > option to boost power scheme  

[more details about capture mode](https://forum.videohelp.com/threads/386069-VirtualDub-FilterMod-updated-capture-mode)

* Generic:
  > Improved video compression dialog  
  > Added builtin DebugMode FrameServer decoder (video, DFSC)  
  > Deleted VDXA option. It was little advantage + lots of trouble. RIP.

# Version 17 summary of changes:

* new supported formats: P010, P016 (input only) 
* new supported formats: v410, Y410, r210, R10k (input/output)  
* new internal filter: DrawText  
* updated tools->test video  
* updated "convert format" filter
* improved realtime profiler
* fixed various colorspace issues
* many other bugfixes

* FFMPEG updated to 2017-08-27  

* updated "master blend" filter  
* updated caching input driver (memory use, bug fixes etc)
* added builtin CineForm support [see notes](https://sourceforge.net/p/vdfiltermod/wiki/cineform)
* added simple x265 encoder  
* added simple VP8 encoder  
* updated x264 encoder (libx264 152)
* updated FFV1 encoder  
* updated prores encoder  
* replaced MPEG-2 plugin with v4.5 (thank isidroco for noticing)

#[Longer history](https://sourceforge.net/p/vdfiltermod/wiki/changes/)
