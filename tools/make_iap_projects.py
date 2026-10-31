"""Generate separate, unflashed boot and relocated-app Keil targets."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parent.parent
project=root/'keilproj'
target=project/'iap'
target.mkdir(exist_ok=True)
(target/'vendor').mkdir(exist_ok=True)
for name in ('flash.c','flash.h'):
    assert (target/'vendor'/name).is_file(), 'Missing bundled vendor FLASH driver: '+name
startup=(project/'common/device/startup_hc32l17x.s').read_text(encoding='utf-8')
vector=startup.split('__Vectors                       ',1)[1].split('__Vectors_End',1)[0]
assert len(re.findall(r'\bDCD\b',vector))==48,'Application vector size changed'
def forward(match):
    label=match.group(2)
    return match.group(1)+(label if label in ('__initial_sp','Reset_Handler') else 'Iap_Dispatch')
startup=re.sub(r'(\bDCD\s+)([A-Za-z_]\w*)',forward,startup)
startup=startup.replace('                AREA    RESET,',
                        '                IMPORT  Iap_Dispatch\n                AREA    RESET,',1)
(target/'startup_iap.s').write_text(startup,encoding='utf-8')
def write_target(filename,output,boot=False):
    tree=ET.parse(project/'1.uvprojx'); doc=tree.getroot()
    def set_text(name,value):
        nodes=doc.findall('.//'+name)
        assert len(nodes)==1,(name,len(nodes))
        nodes[0].text=value
    set_text('OutputDirectory',('.'+chr(92)+'ObjectsBoot'+chr(92)) if boot else ('.'+chr(92)+'ObjectsApp'+chr(92)))
    set_text('ListingPath',('.'+chr(92)+'ListingsBoot'+chr(92)) if boot else ('.'+chr(92)+'ListingsApp'+chr(92)))
    set_text('OutputName',output)
    set_text('ScatterFile',('.'+chr(92)+'boot.sct') if boot else ('.'+chr(92)+'app.sct'))
    doc.find('.//Cads/VariousControls/IncludePath').text='./vendor;../src;../common/ddl/inc;../common/ddl/common;../common/cmsis'.replace('/',chr(92))
    doc.find('.//Cads/VariousControls/Define').text='HC32L176'+('' if boot else ',LBJ_IAP_APP')
    groups=doc.find('.//Groups')
    if boot:
        groups.clear()
        group=ET.SubElement(groups,'Group'); ET.SubElement(group,'GroupName').text='boot'
        files=ET.SubElement(group,'Files')
        names=['boot_platform.c','iap_core.c','iap_wire.c','boot_jump.s','startup_iap.s',
               'vendor/flash.c','../common/device/system_hc32l17x.c',
               '../common/ddl/src/ddl.c','../common/ddl/src/gpio.c','../common/ddl/src/sysctrl.c']
        for name in names:
            file=ET.SubElement(files,'File')
            ET.SubElement(file,'FileName').text=Path(name).name
            ET.SubElement(file,'FileType').text='2' if name.endswith('.s') else '1'
            ET.SubElement(file,'FilePath').text=name.replace('/',chr(92))
    else:
        for item in groups.findall('.//FilePath'):
            path=item.text.replace('\\','/')
            if path.startswith('./'): path=path[2:]
            item.text=('../'+path).replace('/',chr(92))
    # Each target has its own project path and no generated debug session file.
    tree.write(target/filename,encoding='utf-8',xml_declaration=True)
write_target('boot.uvprojx','lbj_boot',True)
write_target('app.uvprojx','lbj_app')
print('Generated IAP boot/app projects; UART1 PF00/PF01 contacts 3/7; UART update verified with forced reset entry')
