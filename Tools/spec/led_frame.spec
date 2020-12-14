# -*- mode: python ; coding: utf-8 -*-

block_cipher = None


a = Analysis(['../led_frame.py'],
             pathex=['E:\\repo\\stm32f207VET6_B\\Tools'],
             binaries=[],
             datas=[],
             hiddenimports=['pony.orm.dbproviders.sqlite', 'simplejson'],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher,
             noarchive=False)

a.datas += [('.\\icos\\tt.ico', 'E:\\repo\\stm32f207VET6_B\\Tools\\icos\\tt.ico', 'DATA')]
a.datas += [('.\\log\\dc201_led.log', 'E:\\repo\\stm32f207VET6_B\\Tools\\log\\dc201_led.log', 'DATA')]
a.datas += [('.\\conf\\led_config.json', 'E:\\repo\\stm32f207VET6_B\\Tools\\conf\\led_config.json', 'DATA')]
a.datas += [('.\\data\\led_sample.sqlite', 'E:\\repo\\stm32f207VET6_B\\Tools\\data\\led_sample.sqlite', 'DATA')]

pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          [],
          exclude_binaries=True,
          name='dc201_led',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=False,
          console=False, 
          icon='E:\\repo\\stm32f207VET6_B\\Tools\\icos\\tt.ico')

coll = COLLECT(exe,
               a.binaries,
               a.zipfiles,
               a.datas,
               strip=False,
               upx=False,
               upx_exclude=[],
               name='dc201_led')
