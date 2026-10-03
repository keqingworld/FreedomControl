"""Extract neutral NPC base identifiers from user-supplied plugins.
No animation files, script implementations, or full plugin records are copied.
Source file IDs are normalized to origin plugin + local form ID; never bake runtime prefixes.
"""
from __future__ import annotations
import argparse,collections,csv,hashlib,json,pathlib,struct
from esp_read import Plugin

def extract(root:pathlib.Path):
    entries={}; races={}; sources=[]
    for path in sorted(root.rglob('*')):
        if path.suffix.lower() not in ('.esp','.esm','.esl'):continue
        p=Plugin(path);count=0; local=set()
        for r in p.records:
            if r['type']!='NPC_':continue
            count+=1
            key=p.key(r['id'])
            if key[0].startswith('INVALIDMASTER'):raise ValueError(f'{path}: {key}')
            if r['flags']&0x20:continue
            fields=dict(r['subs']);race=p.key(struct.unpack('<I',fields.get('RNAM',bytes(4)))[0])
            acbs=fields.get('ACBS',bytes(4));flags=struct.unpack_from('<I',acbs)[0]
            full=fields.get('FULL',b'')
            name=full.rstrip(b'\0').decode('utf-8','replace') if not(p.records[0]['flags']&0x80) else ''
            token=key[0].lower(),key[1];local.add(token)
            e=entries.setdefault(token,{'plugin':key[0],'localID':key[1],'editor':r['edid'],'name':name,
                'sources':[],'racePlugin':race[0],'raceLocalID':race[1], 'preset':bool(flags&4),
                'unique':bool(flags&32),'scripted':False})
            if path.name not in e['sources']:e['sources'].append(path.name)
            if path.name=='DemonicCreatures.esp' or not e['name']:
                e.update(name=name,editor=r['edid'],racePlugin=race[0],raceLocalID=race[1])
            e['scripted']|='VMAD' in fields
            if race[0]:races[race]=True
        if count:sources.append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'NPCRecords':count,'nonDeletedUniqueKeys':len(local),'masters':p.masters})
    es=sorted(entries.values(),key=lambda e:(e['plugin'].lower(),e['localID']))
    return {'schema':1,'build':(pathlib.Path(__file__).resolve().parents[1]/'LATEST_BUILD_ID.txt').read_text(encoding='ascii').strip(),
        'scope':'Neutral NPC_ base records only; not an animation support or active-load-order list.',
        'sources':sources,'entries':es,
        'races':[{'plugin':k[0],'localID':k[1]} for k in sorted(races)]}
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('root',type=pathlib.Path);ap.add_argument('--out',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]/'package/SKSE/Plugins/FreedomControl.creatures.json');args=ap.parse_args()
    doc=extract(args.root);args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(doc,ensure_ascii=False,indent=2),encoding='utf-8')
    print('unique NPC keys',len(doc['entries']),'sources',doc['sources'])
