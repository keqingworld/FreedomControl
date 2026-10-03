"""Author FreedomControlRuntime.esp from documented SSE record structures.
No records, scripts, meshes, or animations from research mods are redistributed.
Run Python 3.10+; the generated ESP is already shipped, so normal builds need no Python.
"""
from __future__ import annotations
import argparse, pathlib, struct, json
U=lambda n:struct.pack('<I',n)
F=lambda n:struct.pack('<f',n)
Z=lambda s:s.encode('utf-8')+b'\0'
SELF=0x01000000
ID={'quest':0x800,'members':0x801,'enemies':0x802,'dragons':0x803,'distance':0x804,
    'cell':0x805,'parking':0x806,'effect':0x807,'spell':0x808,'orbit':0x809,'fallback':0x80A}
DISTANCES=(96,160,240,400,700,1100,1800,3000)
def sub(tag,data=b''):
    if len(data)>65535:return b'XXXX'+struct.pack('<HI',4,len(data))+tag.encode()+b'\0\0'+data
    return tag.encode()+struct.pack('<H',len(data))+data
def rec(tag,fid,body,flags=0):
    return tag.encode()+struct.pack('<IIIIHH',len(body),flags,fid,0,44,0)+body
def group(label,body,kind=0):
    label=label.encode() if isinstance(label,str) else U(label)
    return b'GRUP'+U(24+len(body))+label+struct.pack('<IHHHH',kind,0,0,0,0)+body
def own(n):return SELF+n
def condition(function,param=0,value=1.0,comparison=0):
    # 32-byte CTDA: operator, float comparison, function, two params, run-on, ref, -1.
    return sub('CTDA',struct.pack('<B3xfH2xIII Ii',comparison,value,function,param,0,0,0,-1))
def package_header(edid):
    # Preferred speed + swim; no MustComplete/IgnoreCombat/NoCombatAlert. Disable ambient interrupts.
    return sub('EDID',Z(edid))+sub('PKDT',struct.pack('<IBBBBHH',0x42000,18,0,2,0,0,0))+sub('PSDT',bytes.fromhex('ff ff 00 ff ff 00 00 00 00 00 00 00'))
def events():
    return b''.join(sub(t)+sub('INAM',U(0))+sub('PDTO',bytes(8)) for t in ('POBA','POEA','POCA'))
def follow(fid,edid,distance,conditions=b''):
    body=package_header(edid)+conditions+sub('PKCU',struct.pack('<III',6,0x19B2C,4))
    body+=sub('ANAM',Z('SingleRef'))+sub('PTDA',struct.pack('<III',0,0x14,0))
    for d in (distance,distance+128):body+=sub('ANAM',Z('Float'))+sub('CNAM',F(d))
    for b in (1,0,0):body+=sub('ANAM',Z('Bool'))+sub('CNAM',bytes([b]))
    body+=b''.join(sub('UNAM',bytes([i])) for i in (0,1,2,4,6,8))+sub('XNAM',b'\x09')+events()
    return rec('PACK',own(fid),body)
def faction(fid,name,relations):
    body=sub('EDID',Z(name))+sub('FULL',Z(name))
    # Combat relationship enum: 0 neutral, 1 enemy, 2 ally, 3 friend.
    for other,value,combat in relations:body+=sub('XNAM',struct.pack('<IiI',other,value,combat))
    return rec('FACT',own(fid),body+sub('DATA',U(0)))
def personal_quest(slot):
    body=sub('EDID',Z(f'FC11PersonalQuest{slot:02}'))+sub('FULL',Z(f'keqing - Personal Quest {slot+1}'))
    # User-created template, never auto-started; repeatable stages, side-quest journal entry.
    body+=sub('DNAM',struct.pack('<HBBII',8,0,44,0,8))+sub('NEXT')
    for stage,flags in ((0,0),(10,0),(100,1),(200,2)):
        body+=sub('INDX',struct.pack('<HBB',stage,0,0))+sub('QSDT',bytes([flags]))+sub('CNAM',Z('Personal objective progress managed by FreedomControl.'))
    for i in range(8):
        body+=sub('QOBJ',struct.pack('<H',(i+1)*10))+sub('FNAM',U(0))+sub('NNAM',Z(f'Personal objective {i+1}'))
    body+=sub('ANAM',U(0))
    return rec('QUST',own(0x900+slot),body)

def build():
    chunks=[]; packs=[]; ids=[]
    body=package_header('FC10DragonOrbit')+condition(71,own(ID['dragons']))
    body+=sub('PKCU',struct.pack('<III',4,0x15B84,1))
    body+=sub('ANAM',Z('Location'))+sub('PLDT',struct.pack('<III',0,0x14,50))
    for d in (2200,3600,1400):body+=sub('ANAM',Z('Float'))+sub('CNAM',F(d))
    body+=b''.join(sub('UNAM',bytes([i])) for i in (1,2,3,4))+sub('XNAM',b'\x05')+events()
    packs.append(rec('PACK',own(ID['orbit']),body));ids.append(own(ID['orbit']))
    for band,distance in enumerate(DISTANCES):
        for lane in range(3):
            fid=0x820+band*3+lane
            conditions=condition(74,own(ID['distance']),float(band))+condition(73,own(ID['members']),float(lane))
            packs.append(follow(fid,f'FC10FollowB{band}L{lane}',distance+lane*48,conditions));ids.append(own(fid))
    packs.append(follow(ID['fallback'],'FC10FallbackFollow',240));ids.append(own(ID['fallback']))
    chunks.append(group('GLOB',rec('GLOB',own(ID['distance']),sub('EDID',Z('FC10FollowBand'))+sub('FNAM',b'f')+sub('FLTV',F(2)))))
    chunks.append(group('FACT',b''.join([
        faction(ID['members'],'FC10KeqingLegion',[(0xDB1,1000,2),(own(ID['enemies']),-1000,1)]),
        faction(ID['enemies'],'FC10SpawnedEnemies',[(0xDB1,-1000,1),(own(ID['members']),-1000,1)]),
        faction(ID['dragons'],'FC10FlyingMembers',[])])))
    chunks.append(group('PACK',b''.join(packs)))
    body=sub('EDID',Z('FC10IndependentFollowers'))+sub('FULL',Z('keqing - Independent Followers'))
    body+=sub('DNAM',struct.pack('<HBBII',0,127,44,0,0))+sub('NEXT')+sub('ANAM',U(512))
    for slot in range(512):
        body+=sub('ALST',U(slot))+sub('ALID',Z(f'FCMember{slot:03}'))+sub('FNAM',U(0x129A))
        # Never make actor essential or protected. Never reserve other quests' actors.
        body+=b''.join(sub('ALPC',U(p)) for p in ids)+sub('VTCK',U(0))+sub('ALED')
    chunks.append(group('QUST',rec('QUST',own(ID['quest']),body)+b''.join(personal_quest(i) for i in range(64))))
    # Persistent parking reference for released aliases. No ForceRefIntoAlias(nullptr).
    cell=rec('CELL',own(ID['cell']),sub('EDID',Z('FC10InternalParkingCell'))+sub('FULL',Z('FreedomControl internal'))+sub('DATA',struct.pack('<H',1)))
    marker=rec('REFR',own(ID['parking']),sub('EDID',Z('FC10AliasParkingMarker'))+sub('NAME',U(0x3B))+sub('DATA',struct.pack('<6f',0,0,0,0,0,0)),0x400)
    subblock=cell+group(own(ID['cell']),group(own(ID['cell']),marker,8),6)
    chunks.append(group('CELL',group(ID['cell']%10,group((ID['cell']//10)%10,subblock,3),2)))
    # Inert script-archetype self effect: actual action is handled only by SKSE spell event.
    data=bytearray(152)
    struct.pack_into('<I',data,0,0x0C008C10) # no magnitude/area, hide UI, painless/no hit effect
    for pos in (12,16,68,88):struct.pack_into('<i',data,pos,-1)
    struct.pack_into('<I',data,64,1) # Script archetype, NOT Summon Creature (18)
    struct.pack_into('<I',data,80,1) # Fire and Forget
    struct.pack_into('<I',data,84,0) # Self
    struct.pack_into('<I',data,140,2) # silent
    effect=rec('MGEF',own(ID['effect']),sub('EDID',Z('FC10KeqingPulseEffect'))+sub('FULL',Z('keqing - Clear Zone'))+sub('DATA',bytes(data))+sub('DNAM',Z('Native configurable actor clear pulse.')))
    chunks.append(group('MGEF',effect))
    spit=struct.pack('<III f II ff I',0,1,3,0,1,0,0,0,0)
    spell=rec('SPEL',own(ID['spell']),sub('EDID',Z('FC10KeqingClearZone'))+sub('OBND',bytes(12))+sub('FULL',Z('keqing - Clear Zone'))+sub('ETYP',U(0x25BCE))+sub('DESC',Z('Removes non-player actors in the radius configured in FreedomControl.'))+sub('SPIT',spit)+sub('EFID',U(own(ID['effect'])))+sub('EFIT',struct.pack('<fII',0,0,1)))
    chunks.append(group('SPEL',spell))
    # HEDR numRecords includes GRUP records, as in CK-produced files.
    body=b''.join(chunks)
    def count(b):
        n=0;p=0
        while p<len(b):
            n+=1;sz=struct.unpack_from('<I',b,p+4)[0]
            if b[p:p+4]==b'GRUP':n+=count(b[p+24:p+sz]);p+=sz
            else:p+=24+sz
        assert p==len(b);return n
    header=rec('TES4',0,sub('HEDR',struct.pack('<fII',1.7,count(body),0x940))+sub('CNAM',Z('FreedomControl'))+sub('SNAM',Z('FC-0.5.0-KERNEL11-20260925-G; authored runtime records; Skyrim.esm only.'))+sub('MAST',Z('Skyrim.esm'))+sub('DATA',bytes(8)),0x200)
    return header+body
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]/'package/FreedomControlRuntime.esp')
    args=ap.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build());print(args.output)
