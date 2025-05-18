### `readme.md`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/plugins/remove%20frames/readme.md/download`
- Upload date: `2020-02-02 18:21:12 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/plugins/remove-frames/readme.md


### `remove_frames.vdf`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/plugins/remove%20frames/remove_frames.vdf/download`
- Upload date: `2020-02-02 18:16:22 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/plugins/remove-frames/remove_frames.vdf


### `remove_frames_src.zip`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/plugins/remove%20frames/remove_frames_src.zip/download`
- Upload date: `2020-02-02 18:15:46 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/plugins/remove-frames/remove_frames_src.zip


### `remove_frames64.vdf`

- URL: `https://sourceforge.net/projects/vdfiltermod/files/plugins/remove%20frames/remove_frames64.vdf/download`
- Upload date: `2020-02-02 18:14:54 UTC`
- Mirror: https://github.com/vdfiltermod/files/releases/download/plugins/remove-frames/remove_frames64.vdf

---

This filter can delete frames according to provided list or based on pixel difference (to delete duplicate frames).  

This is example of 2-pass filter if you want to make one:  
1st pass (analyze) compares frames and remembers results (can be saved with project).  
2nd pass is frame manipulation (typical work for filter).  

You can use "analyze" or provide your own list of frames instead. These are 2 independent choices.  

Threshold is acceptable pixel difference (range 0-255).  
0 : pixels must be completely identical to flag frame as duplicate  
255: accept anything (delete all frames)  

