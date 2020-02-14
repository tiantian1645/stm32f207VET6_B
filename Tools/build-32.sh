source venv32/Scripts/activate && \
python -c 'import struct; import sys; print(sys.version_info[:], 8 * struct.calcsize("P"))' | cat && \
pyinstaller --clean --distpath ./dist/32 --workpath ./build/32  --noconfirm --noupx qt_frame.spec && \
/c/Program\ Files\ \(x86\)/7-Zip/7z.exe a -t7z -m0=LZMA2 -mmt=on -mx9 -md=64m -mfb=64 -ms=4g -mqs=on -sccUTF-8 -bb0 -bse0 -bsp2 -sfx7z.sfx \
    "-wE:\WebServer\DC201\调试上位机" "E:\WebServer\DC201\调试上位机\dc201调试上位机-x86.exe" "C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Tools\dist\32\dc201"
