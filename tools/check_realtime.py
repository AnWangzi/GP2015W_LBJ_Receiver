"""Guard the active firmware against reintroducing delay/poll-wait calls."""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
for path in (root/'keilproj/src').glob('*.c'):
    source=re.sub(r'/\*.*?\*/|//[^\n]*','',path.read_text(encoding='utf-8'),flags=re.S)
    assert not re.search(r'\bdelay\w*\s*\(',source), path
graph=(root/'keilproj/Objects/lcdtest.htm').read_text(errors='replace')
loaded=set(re.findall(r'<P><STRONG><a name="[^\"]+"></a>([^<]+)</STRONG>',graph))
assert loaded, 'Missing Keil call graph'
for name in ('delay1ms','delay100us','delay10us','SI446X_WaitCTS','SI446X_CmdRespN','SI446X_ApplyConfig'):
    assert name not in loaded, 'Blocking legacy routine linked: '+name
print('PASS active firmware: no delay calls, synchronous CTS/config routines absent from linked call graph')
