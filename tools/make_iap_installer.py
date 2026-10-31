"""Wrap the verified complete IAP image in a flash-only Keil AXF target."""
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
from serial_update import validate_image
root=Path(__file__).resolve().parent.parent
folder=root/'keilproj/iap'
image=(folder/'release/first-install.bin').read_bytes()
boot=(folder/'ObjectsBoot/lbj_boot.bin').read_bytes()
app=(folder/'ObjectsApp/lbj_app.bin').read_bytes()
assert len(image)==0x20000 and image[:len(boot)]==boot
assert image[0x2000:0x2000+len(app)]==app
validate_image(app)
entry=struct.unpack_from('<I',boot,4)[0]&~1
def emit_data(data):
    lines=[]
    for offset in range(0,len(data),32):
        part=data[offset:offset+32]
        words=struct.unpack('<%dH'%(len(part)//2),part)
        lines.append('        DCW '+', '.join('0x%04X'%word for word in words))
    return lines
# Declare the real reset instruction as code so the data-wrapper AXF has a
# valid Thumb entry point. Every emitted halfword still equals the BIN.
lines=['; Generated self-written IAP boot/app image, not original firmware.',
       '        AREA IAP_INSTALL, CODE, READONLY, ALIGN=2', '        THUMB']
lines+=emit_data(image[:entry])
lines+=['        EXPORT __iap_install_entry', '__iap_install_entry PROC',
        '        DCI 0x%04X'%struct.unpack_from('<H',image,entry)[0], '        ENDP']
lines+=emit_data(image[entry+2:]); lines+=['        END','']
(folder/'install_image.s').write_text('\n'.join(lines),encoding='ascii')
(folder/'install.sct').write_text(
    'LR_INSTALL 0x00000000 0x00020000 {\n'
    '  ER_INSTALL 0x00000000 0x00020000 { *(IAP_INSTALL) }\n}\n',encoding='ascii')
tree=ET.parse(root/'keilproj/1.uvprojx'); doc=tree.getroot()
for tag,value in {'OutputDirectory':'.\\ObjectsInstall\\','ListingPath':'.\\ListingsInstall\\',
                  'OutputName':'lbj_install','ScatterFile':'.\\install.sct'}.items():
    nodes=doc.findall('.//'+tag); assert len(nodes)==1; nodes[0].text=value
doc.find('.//LDads/Misc').text='--entry __iap_install_entry'
doc.find('.//Cads/VariousControls/IncludePath').text=''
groups=doc.find('.//Groups'); groups.clear()
group=ET.SubElement(groups,'Group'); ET.SubElement(group,'GroupName').text='IAP complete installer'
files=ET.SubElement(group,'Files'); item=ET.SubElement(files,'File')
ET.SubElement(item,'FileName').text='install_image.s'
ET.SubElement(item,'FileType').text='2'
ET.SubElement(item,'FilePath').text='.\\install_image.s'
# No debug session is needed to flash this data-only installation image.
doc.find('.//UpdateFlashBeforeDebugging').text='0'
tree.write(folder/'install.uvprojx',encoding='utf-8',xml_declaration=True)
# Reuse only the verified ST-Link connection settings, without auto-debug scripts.
local_options=root/'keilproj/1.uvoptx'
if local_options.exists():
    opts=ET.parse(local_options); opt=opts.getroot()
    for node in opt.findall('.//tIfile'): node.text=''
    for tag in ('tLdApp','tGomain','tRbreak','tRwatch','tRmem','tRfunc','tRbox'):
        for node in opt.findall('.//'+tag): node.text='0'
    for node in opt.findall('.//Breakpoint'): node.clear()
    opts.write(folder/'install.uvoptx',encoding='utf-8',xml_declaration=True)
print('Generated self-written IAP install target; 128 KiB complete image, no flashing performed')
