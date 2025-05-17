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

# Version 20:

update 19 (build 44282)

* fixed bug in resize filter (wrong chroma cropping)  
* auto-size option in filter preview  
* display selection duration in statusbar  
* non-standard AVI option (#310)  
* other fixes  

update 18 (build 44237)

* fixed memory leak in caching input driver (#294)  
* fixed File Open dialog for raw video (#297)  
* updated curves GUI  

update 17 (build 44065)

* fixed invalid AVI when using "save segmented"  
* fixed [#293] (crash in capture)  
* fixed [#292] (avoid AddRange script)  
* draw text/draw time filters: font was scaled by system DPI  

update 16 (build 44013)

* fixed [#186] (display bug in capture)  
* fixed some errors in "master blend" plugin  

update 15 (build 43943)

* added internal filter "draw time"    
* fixed [#277] (job reload error)  
* added command line options: see comments [#268]  
* added filter command "Duplicate Filter"  

update 14 (build 43803)

* fixed [#256]  
* enabled FITS format decoding  

update 13 (build 43786)

* fixed [#246] (frame rate option)  
* added b48r format support  
* filter levels: fixed some errors, added full range support  
* added f32 (rgba) support to filters: levels, resize, gamma, fill, 6-axis  
* other fixes  

update 12 (build 43702)

* cleanup in File menu, moved some items to other menus  
* remember recently captured files  
* fixed bugs in "Open video" dialog  
* fixed: audio truncated when using offset  
* other fixes  

update 11 (build 43602)

* new filter: [canvas size](https://sourceforge.net/p/vdfiltermod/wiki/canvas_size/)  
* [changed filter "fill"](https://sourceforge.net/p/vdfiltermod/wiki/fill/): new "letterbox" option  
* polishing in hotkey editor  
* polishing in "save image" procedure  
* added gray TGA support  
* other fixes  

update 10 (build 43385)

* fixed #224 (broken dib export)  

update 9 (build 43382)

* updated x264 encoder (libx264 157)  
* added x264 grayscale option  
* added/fixed support for some uncompressed formats in mov,mp4,mkv,nut  

update 8 (build 43364)

* updated preferences:
  some items deleted or moved to better place  
  added main : Remember autosize and zoom  
  added timeline : time format  
  added timeline : buttons scale etc  
* updated timeline control  
* added support to read/write planar rgb formats (works with AviSynth+, MagicYUV)  
* enabled writing uncompressed video in NUT container  
* added gray formats to codec: FFV1, FFVHUFF  
* added mp4 faststart option  
* fixed bug reading VirtualDub.ini (corrupt unicode characters)  
* fixed #214, #216, #217  

update 7 (build 43073)

* fixed image sequence opened as single image  
* autosave before running jobs  
* minor polishing  

update 6 (build 43048)

* improved audio display [wiki/AudioDisplay](https://sourceforge.net/p/vdfiltermod/wiki/AudioDisplay/)  
* fixed some file dialogs  
* fixed #202, #196, #201, other bugs  

update 5 (build 42711)

* previous build crashed in "save video" dialog  

update 4 (build 42699)

* updated "save image sequence" function  
* updated auxsetup.exe  
* fixed some bugs  

update 3 (build 42671)

* added audio compression: flac, alac, opus, vorbis  
* added File->Save audio: wav, mka, m4a, aiff  
* fixed bugs with audio interleaving, encoder delay etc.  

update 2 (build 42475)

* updated grayscale filter  
* some optimizations  
* fixed another possible deadlock in processing  
* fixed bug: #179  

update 1 (build 42338)

* added internal formats: planar rgb/rgba (8bit, 16bit, float)  
* improved gray/gray16 support: added to tiff, external encoder config  
* updated "go to frame" dialog  
* remember panes zoom  
* added preferences: render options, timeline options, avi options  
* override preferences from script  
* other small fixes  
* cineform: experimental raw bayer import/export, see  [wiki/cineform](https://sourceforge.net/p/vdfiltermod/wiki/cineform/)
* x265 encoder: added tune options  

#[Longer history](https://sourceforge.net/p/vdfiltermod/wiki/changes/)
