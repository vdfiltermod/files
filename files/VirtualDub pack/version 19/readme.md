### `VirtualDub2_41493.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41493.zip/download`
- Upload date: `2018-08-13 21:22:54 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41493.zip


### `readme.md`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/readme.md/download`
- Upload date: `2018-08-13 21:18:09 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/readme.md


### `VirtualDub2_41867.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41867.zip/download`
- Upload date: `2018-07-22 22:22:45 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41867.zip


### `VirtualDub2_41980.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41980.zip/download`
- Upload date: `2018-06-10 19:19:56 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41980.zip


### `VirtualDub2_42154.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_42154.zip/download`
- Upload date: `2018-06-04 21:07:11 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_42154.zip


### `VirtualDub2_42087.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_42087.zip/download`
- Upload date: `2018-05-30 07:38:21 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_42087.zip


### `VirtualDub2_41882.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41882.zip/download`
- Upload date: `2018-05-06 18:18:21 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41882.zip


### `VirtualDub2_41585.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41585.zip/download`
- Upload date: `2018-04-29 08:21:58 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41585.zip


### `VirtualDub2_41552.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41552.zip/download`
- Upload date: `2018-04-04 08:48:01 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41552.zip


### `VirtualDub2_41768.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41768.zip/download`
- Upload date: `2018-03-30 23:30:10 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41768.zip


### `VirtualDub2_41462.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41462.zip/download`
- Upload date: `2018-03-18 02:55:03 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41462.zip


### `VirtualDub2_41976.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/VirtualDub%20pack/version%2019/VirtualDub2_41976.zip/download`
- Upload date: `2018-03-11 13:08:07 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/VirtualDub-pack/version-19/VirtualDub2_41976.zip

---

#[Overview](https://sourceforge.net/p/vdfiltermod/wiki/)

Includes both **x86 and x64 binaries**  
Includes plugins:

*  x264 encoder
*  lagarith encoder
*  avlib-1 (collection of various plugins based on FFMPEG)
  > caching input driver (2.1)  
  > video compression to Huffyuv, FFV1, Prores, VP8, VP9, H265, CineForm  
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

# Version 19:

update 11 (build 42154)

* fixed #163 (crash in filters preview)
* fixed #159 (broken export of gif, apng, png, bmp, tga)
* fixed bugs: #158, #161, other
* experimental raw bayer import/export with CineForm

update 10 (build 42087)

* added cpu usage graph to realtime profiler  
* capture: show overall cpu usage, small improvements in GDI screen capture  
* fix #156  
* fix frameserver output  

update 9 (build 41980)

* fix #135 (external encoders process hanging) 
* fix crash when decoding audio in some cases  

update 8 (build 41976)

* allow any uncompressed format in capture  
* fix #137 (output conversion bug)  
* fix #136  

update 7 (build 41882)

* conversion errors in capture mode  

update 6 (build 41867)

* fixed ambiguity in pixel format dialogs  
  decoding: apply new format immediately to see if it really works  
  output: show actual format applied to vfw codecs  
  show related formats (input, filters)  

update 5 (build 41768)

* fixed many internal format conversion inconsistencies (offset CbCr etc.)  
* added YCbCr+Alpha support: resize, DrawText  
* other small fixes  
* FFV1: added new formats (GBRAP16,...)  
* HEVC: added lossless  
* added simple VP9 export (8/10/12-bit)  

update 4 (build 41585)

* sort filters by author, module  
* fix missing dot (#106)  

update 3 (build 41552)

* export with external encoder:  
  > show command line in log  
  > set current directory (fix for %(outputname), #107)  
  > added to Batch Wizard  
* added options to Batch Wizard (add/delete files)  
* replaced directory selection dialogs on Win7 and above  
* fixed bug in capture menu  

update 2 (build 41493)

fixed bugs:
  > AVI DV issues (#98)  
  > GammaCorrect loading (#100)  
  > full screen switching (#99)  

update 1 (build 41462)

* added YUV+Alpha formats  
  > enabled in video compression: Huffyuv, FFV1, Prores  
* improved performance with CineForm (using 16-bit yuv option)  
* improved/added format support:  
  > Y416 (enabled alpha)  
  > v308  
  > YU64  
  > b64a (works in MOV)  
* added option: "Go->Zoom selection"  
* improved input/output panes:  
  > borderless when possible  
  > Full screen option  
  > Pan & zoom alignment option  
* new navigation keys: fast forward/backward  
* selectable start-end range for video filter 
* updated internal filters: Gamma correct, Invert  

#[Longer history](https://sourceforge.net/p/vdfiltermod/wiki/changes/)
