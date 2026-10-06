# Build on Windows for CardGame.exe: pyinstaller --noconfirm CardGame.spec
from pathlib import Path
root = Path(SPECPATH)
a = Analysis([str(root / 'main.py')], pathex=[str(root)], binaries=[],
             datas=[(str(root / 'data'), 'data'), (str(root / 'assets'), 'assets'),
                    (str(root / 'config.json'), '.')],
             hiddenimports=[], hookspath=[], hooksconfig={}, runtime_hooks=[], excludes=[])
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, a.binaries, a.datas, [], name='CardGame',
          debug=False, bootloader_ignore_signals=False, strip=False, upx=False, console=False)
