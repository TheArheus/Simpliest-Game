@echo off
if not defined DevEnvDir (
    call vcvarsall x64
)
REM call vcvarsall x64

set CommonCompileFlags=-MTd -nologo -fp:fast -WX- -W4 -Gm- -GR- -EHa- -Od -Oi -wd4505 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_WIN32=1 -FC -Z7 
set CommonLinkFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

del *.pdb > NUL 2> NUL
echo WAITING FOR PDB > lock.pdb
cl %CommonCompileFlags% ..\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -opt:ref -PDB:handmade_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender 
del lock.pdb
cl %CommonCompileFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkFlags%
popd
