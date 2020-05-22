version=`python -c "import version; print(version.VERSION_FA)"`

source venv/Scripts/activate && \
python -c 'import struct; import sys; print(sys.version_info[:], 8 * struct.calcsize("P"))' | cat && \
rm -rf ./dist_fa/64/* && \
pyinstaller --clean --distpath ./dist_fa/64 --workpath ./build/64 --noconfirm --noupx ./spec/factory.spec && \
rm -f ./dist_fa/64/dc201_fa/conf/* ./dist_fa/64/dc201_fa/log/* && \
/d/\Program\ Files/PeaZip/res/7z/7z.exe a -t7z -m0=LZMA2 -mmt=on -mx9 -md=64m -mfb=64 -ms=4g -mqs=on -sccUTF-8 -bb0 -bse0 -bsp2 -sfx7z.sfx \
    "-wE:\WebServer\DC201\工装上位机" "E:\WebServer\DC201\工装上位机\dc201工装上位机-amd64_${version}.exe" "C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Tools\dist_fa\64\dc201_fa"



source venv32/Scripts/activate && \
python -c 'import struct; import sys; print(sys.version_info[:], 8 * struct.calcsize("P"))' | cat && \
rm -rf ./dist_fa/32/* && \
pyinstaller --clean --distpath ./dist_fa/32 --workpath ./build/32  --noconfirm --noupx ./spec/factory.spec && \
rm -f ./dist_fa/32/dc201_fa/conf/* ./dist_fa/32/dc201_fa/log/* && \
/c/Program\ Files\ \(x86\)/7-Zip/7z.exe a -t7z -m0=LZMA2 -mmt=on -mx9 -md=64m -mfb=64 -ms=4g -mqs=on -sccUTF-8 -bb0 -bse0 -bsp2 -sfx7z.sfx \
    "-wE:\WebServer\DC201\工装上位机" "E:\WebServer\DC201\工装上位机\dc201工装上位机-x86_${version}.exe" "C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Tools\dist_fa\32\dc201_fa"
