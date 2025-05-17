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
