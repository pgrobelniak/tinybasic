100 REM "Screen size"
110 SW=640
120 SH=480 
130 REM "Ball diameter"
140 D=50
150 REM "Ball speed in pixels per sec"
160 S=200
170 REM "Frame time for 60 fps"
180 FT=1000/60
190 REM "Initial position"
200 X=(SW-D)/2: Y=(SH-D)/2 
210 REM "Initial velocity"
220 VX=S: VY=S 
230 REM "Last frame time"
240 L=@T
300 REM "Frame start"
310 REM "Disable drawing
320 SET 10,1
330 REM "Calculate elapsed time"
340 T=@T: E=T-L: L=T
350 REM "Delay if elapsed time less than frame time"
360 IF E<FT THEN DELAY FT-E
370 REM "Apply velocity"
380 X=X+VX*E/1000: Y=Y+VY*E/1000
390 REM "Bounce if out of screen"
400 IF X+D=>SW THEN X=SW-D: VX=-S
410 IF X<0 THEN X=0: VX=S
420 IF Y+D=>SH THEN Y=SH-D: VY=-S
430 IF Y<0 THEN Y=0: VY=S
440 REM "Clear screen"
450 COLOR 255,255,255
460 FRECT 0,0,SW,SH
470 REM "Draw ball"
480 COLOR 0,0,0
490 FCIRCLE X+D/2,Y+D/2,D/2
500 REM "Enable drawing (show frame)"
510 SET 10,0
520 GOTO 300
