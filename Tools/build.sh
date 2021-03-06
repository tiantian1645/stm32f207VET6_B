version=`python -c "import version; print(version.VERSION)"` && \

source venv/Scripts/activate && \

python -c 'import struct; import sys; print(sys.version_info[:], 8 * struct.calcsize("P"))' | cat && \

rm -rf ./dist/64/* && \

pyinstaller --clean --distpath ./dist/64 --workpath ./build/64 --noconfirm --noupx ./spec/qt_frame.spec && \

rm -f ./dist/64/dc201/data/* ./dist/64/dc201/conf/* ./dist/64/dc201/log/* && \

/c/\Program\ Files/PeaZip/res/7z/7z.exe a -t7z -m0=LZMA2 -mmt=on -mx9 -md=64m -mfb=64 -ms=4g -mqs=on -sccUTF-8 -bb0 -bse0 -bsp2 -sfx7z.sfx \
    "-wE:\WebServer\DC201\调试上位机" "E:\WebServer\DC201\调试上位机\dc201调试上位机-amd64_${version}.exe" "E:\repo\stm32f207VET6_B\Tools\dist\64\dc201"
