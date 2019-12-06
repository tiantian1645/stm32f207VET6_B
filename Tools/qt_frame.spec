# -*- mode: python ; coding: utf-8 -*-

block_cipher = None


a = Analysis(['qt_frame.py'],
             pathex=['C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools'],
             binaries=[],
             datas=[],
             hiddenimports=['sqlalchemy.ext.baked'],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher,
             noarchive=False)

a.datas += [('.\\icos\\tt.ico', 'C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools\\icos\\tt.ico', 'DATA')]
a.datas += [('.\\log\\dc201.log', 'C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools\\log\\dc201.log', 'DATA')]
a.datas += [('.\\conf\\config.json', 'C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools\\conf\\config.json', 'DATA')]
a.datas += [('.\\data\\db.sqlite3', 'C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools\\data\\db.sqlite3', 'DATA')]

pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          [],
          exclude_binaries=True,
          name='dc201',
          debug=False,
          bootloader_ignore_signals=False,
          strip=False,
          upx=False,
          console=False, 
          icon='C:\\Users\\Administrator\\STM32CubeIDE\\workspace_1.0.2\\stm32f207VET6_B\\Tools\\icos\\tt.ico')

coll = COLLECT(exe,
               a.binaries,
               a.zipfiles,
               a.datas,
               strip=False,
               upx=False,
               upx_exclude=[],
               name='dc201')
