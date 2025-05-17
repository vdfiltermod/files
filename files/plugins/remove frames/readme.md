This filter can delete frames according to provided list or based on pixel difference (to delete duplicate frames).  

This is example of 2-pass filter if you want to make one:  
1st pass (analyze) compares frames and remembers results (can be saved with project).  
2nd pass is frame manipulation (typical work for filter).  

You can use "analyze" or provide your own list of frames instead. These are 2 independent choices.  

Threshold is acceptable pixel difference (range 0-255).  
0 : pixels must be completely identical to flag frame as duplicate  
255: accept anything (delete all frames)  

