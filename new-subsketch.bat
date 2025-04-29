@echo off
setlocal EnableDelayedExpansion

:: Ask for a sketch name
set /p sketchname=Enter sketch name (no spaces): 

:: Get timestamp (optional)
for /f %%a in ('wmic os get localdatetime ^| find "."') do set dt=%%a
set dt=%dt:~0,4%-%dt:~4,2%-%dt:~6,2%_%dt:~8,2%%dt:~10,2%
set folder=%sketchname%_%dt%

:: Create the folder
mkdir "%folder%"

:: Create the .ino file inside
(
echo // Sketch: %sketchname%
echo void setup() {
echo.    Serial.begin(9600);
echo.    Serial.println("Sketch %sketchname% running...");
echo }
echo.
echo void loop() {
echo.    delay(1000);
echo }
) > "%folder%\%sketchname%.ino"

:: Add to git and commit
git add "%folder%"
git commit -m "Add new sketch: %sketchname%"

:: Optionally open it with Arduino (comment out if not needed)
start "" "arduino://open/?url=file://%cd%\%folder%\%sketchname%.ino"

echo Created sketch in: %folder%\%sketchname%.ino
pause
