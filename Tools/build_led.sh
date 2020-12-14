version=`python -c "import version; print(version.VERSION_LED)"` && \

source venv/Scripts/activate && \

python -c 'import struct; import sys; print(sys.version_info[:], 8 * struct.calcsize("P"))' | cat && \

rm -rf ./dist_led/64/* && \

pyinstaller --clean --distpath ./dist_led/64 --workpath ./build_led/64 --noconfirm --noupx ./spec/led_frame.spec && \

rm -f ./dist_led/64/dc201_led/data/* ./dist_led/64/dc201_led/conf/* ./dist_led/64/dc201_led/log/* && \

/c/\Program\ Files/PeaZip/res/7z/7z.exe a -t7z -m0=LZMA2 -mmt=on -mx9 -md=64m -mfb=64 -ms=4g -mqs=on -sccUTF-8 -bb0 -bse0 -bsp2 -sfx7z.sfx \
    "-wE:\WebServer\DC201\LED工装上位机" "E:\WebServer\DC201\LED工装上位机\dc201-LED工装上位机-amd64_${version}.exe" "E:\repo\stm32f207VET6_B\Tools\dist_led\64\dc201_led"
