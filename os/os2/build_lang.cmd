rem
rem Building language files...
rem
cd ..
strgen\strgen
for %%f in (lang\*.txt) do strgen\strgen %%f
cd strgen

