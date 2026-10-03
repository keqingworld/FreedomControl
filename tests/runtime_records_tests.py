"""Validate generated record structure and curated base-ID metadata, not Skyrim loading."""
from pathlib import Path
import hashlib, importlib.util, json, struct, sys, unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from esp_read import Plugin
import generate_runtime

class RuntimeRecords(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path=ROOT/'package/FreedomControlRuntime.esp'
        cls.plugin=Plugin(cls.path)
        cls.catalog=json.loads((ROOT/'package/SKSE/Plugins/FreedomControl.creatures.json').read_text())
    def test_catalog_build_matches_source(self):
        self.assertEqual(self.catalog['build'],(ROOT/'LATEST_BUILD_ID.txt').read_text().strip())
    def types(self,t):return [r for r in self.plugin.records if r['type']==t]
    def sub(self,r,k):return next(v for n,v in r['subs'] if n==k)
    def test_01_deterministic_authored_plugin(self):
        self.assertEqual(self.path.read_bytes(),generate_runtime.build())
        self.assertEqual(self.plugin.masters,['Skyrim.esm'])
        self.assertEqual(self.plugin.records[0]['flags'] & 0x200,0x200)
    def test_02_only_new_compact_records(self):
        records=self.plugin.records[1:]
        self.assertEqual(len(records),99)
        ids=[r['id'] for r in records];self.assertEqual(len(ids),len(set(ids)))
        self.assertTrue(all(i>>24==1 and 0x800<=i&0xffffff<=0xfff for i in ids))
        self.assertFalse(self.types('NPC_'));self.assertFalse(any(k=='VMAD' for r in records for k,v in r['subs']))
    def test_03_512_independent_reference_aliases(self):
        r=self.types('QUST')[0];d=self.sub(r,'DNAM')
        self.assertEqual(len(d),12);self.assertEqual(d[2],127);self.assertEqual(d[3],44)
        subs=r['subs'];self.assertEqual(sum(k=='ALST' for k,v in subs),512)
        self.assertEqual(sum(k=='ALED' for k,v in subs),512)
        self.assertEqual(sum(k=='ALPC' for k,v in subs),512*26)
        self.assertEqual([struct.unpack('<I',v)[0] for k,v in subs if k=='ALST'],list(range(512)))
        for k,v in subs:
            if k=='FNAM':
                flags=struct.unpack('<I',v)[0];self.assertEqual(flags,0x129a)
                self.assertFalse(flags&1) # reserve flag deliberately absent
        self.assertEqual(struct.unpack('<I',self.sub(r,'ANAM'))[0],512)
    def test_04_follow_template_procedure_data(self):
        for r in self.types('PACK'):
            dat=self.sub(r,'PKCU');n,template,version=struct.unpack('<III',dat)
            self.assertEqual(sum(k=='ANAM' for k,v in r['subs']),n)
            self.assertEqual(len(self.sub(r,'PKDT')),12);self.assertEqual(len(self.sub(r,'PSDT')),12)
            flags=struct.unpack_from('<I',self.sub(r,'PKDT'))[0];self.assertEqual(flags,0x42000)
            if r['edid']=='FC10DragonOrbit':
                self.assertEqual((n,template,version),(4,0x15b84,1))
                self.assertEqual(struct.unpack('<III',self.sub(r,'PLDT')),(0,0x14,50))
            else:
                self.assertEqual((n,template,version),(6,0x19b2c,4))
                self.assertEqual(struct.unpack('<III',self.sub(r,'PTDA')),(0,0x14,0))
                self.assertEqual([v[0] for k,v in r['subs'] if k=='UNAM'],[0,1,2,4,6,8])
            for k,v in r['subs']:
                if k=='CTDA':self.assertEqual(len(v),32)
    def test_05_alias_package_references_resolve(self):
        valid={r['id'] for r in self.types('PACK')}
        for k,v in self.types('QUST')[0]['subs']:
            if k=='ALPC':self.assertIn(struct.unpack('<I',v)[0],valid)
    def test_06_parking_reference_is_persistent(self):
        ref=self.types('REFR')[0]
        self.assertTrue(ref['flags'] & 0x400)
        self.assertEqual(struct.unpack('<I',self.sub(ref,'NAME'))[0],0x3b)
    def test_07_clear_power_is_not_summon_or_damage(self):
        data=self.sub(self.types('MGEF')[0],'DATA');self.assertEqual(len(data),152)
        self.assertEqual(struct.unpack_from('<I',data,64)[0],1)
        s=self.types('SPEL')[0];spit=self.sub(s,'SPIT');self.assertEqual(len(spit),36)
        self.assertEqual(struct.unpack_from('<I',spit,8)[0],3)
        self.assertEqual(struct.unpack('<I',self.sub(s,'EFID'))[0],self.types('MGEF')[0]['id'])
    def test_08_catalog_key_uniqueness(self):
        c=self.catalog;self.assertEqual(c['schema'],1)
        self.assertEqual(c['build'],(ROOT/'LATEST_BUILD_ID.txt').read_text().strip())
        keys=[(e['plugin'].lower(),e['localID']) for e in c['entries']]
        self.assertEqual(len(keys),1097);self.assertEqual(len(set(keys)),1097)
        self.assertTrue(all(0<e['localID']<=0xffffff and 'INVALIDMASTER' not in e['plugin'] for e in c['entries']))
    def test_09_dc_entries_not_lost(self):
        self.assertEqual(sum('DemonicCreatures.esp' in e['sources'] for e in self.catalog['entries']),736)
        counts={s['file']:s['NPCRecords'] for s in self.catalog['sources']}
        self.assertEqual(counts,{'CreatureSummoner.esp':160,'DemonicCreatures.esp':736,'MoreNastyCritters.esp':197,'SLDragons.esp':6})
    def test_10_no_runtime_load_prefix_hardcoding(self):
        for e in self.catalog['entries']:
            self.assertLess(e['localID'],0x1000000)
            self.assertNotIn('fullFormID',e)
    def test_11_no_erotic_assets_or_third_party_scripts(self):
        for p in (ROOT/'package').rglob('*'):
            if p.is_file():self.assertIn(p.suffix.lower(),{'.esp','.json','.ini'})
    def test_12_plugin_form_count_includes_groups(self):
        b=self.path.read_bytes()
        def walk(start,end):
            p=start;n=0
            while p<end:
                self.assertGreaterEqual(end-p,24);size=struct.unpack_from('<I',b,p+4)[0];n+=1
                if b[p:p+4]==b'GRUP':
                    self.assertGreaterEqual(size,24);self.assertLessEqual(p+size,end)
                    n+=walk(p+24,p+size);p+=size
                else:p+=24+size
            self.assertEqual(p,end);return n
        header=self.plugin.records[0];start=24+len(header['body'])
        self.assertEqual(struct.unpack_from('<I',self.sub(header,'HEDR'),4)[0],walk(start,len(b)))

    def test_13_personal_slot_ids_are_stable_and_compact(self):
        quests=self.types('QUST')
        self.assertEqual(len(quests),65)
        personal=quests[1:]
        self.assertEqual([r['id'] for r in personal],[0x01000900+i for i in range(64)])
        self.assertEqual([r['edid'] for r in personal],[f'FC11PersonalQuest{i:02}' for i in range(64)])
        self.assertEqual(struct.unpack_from('<I',self.sub(self.plugin.records[0],'HEDR'),8)[0],0x940)
    def test_14_personal_templates_never_auto_start(self):
        for r in self.types('QUST')[1:]:
            flags,priority,version,delay,qtype=struct.unpack('<HBBII',self.sub(r,'DNAM'))
            self.assertEqual((flags,priority,version,delay,qtype),(8,0,44,0,8))
            self.assertFalse(flags&1)
            self.assertEqual(struct.unpack('<I',self.sub(r,'ANAM'))[0],0)
            self.assertFalse(any(k in ('ALST','VMAD','QSTA') for k,v in r['subs']))
    def test_15_personal_stages_have_completion_flags(self):
        for r in self.types('QUST')[1:]:
            self.assertEqual([struct.unpack('<HBB',v) for k,v in r['subs'] if k=='INDX'],[(0,0,0),(10,0,0),(100,0,0),(200,0,0)])
            self.assertEqual([v for k,v in r['subs'] if k=='QSDT'],[b'\0',b'\0',b'\1',b'\2'])
    def test_16_personal_journal_has_eight_original_goals(self):
        for r in self.types('QUST')[1:]:
            self.assertEqual([struct.unpack('<H',v)[0] for k,v in r['subs'] if k=='QOBJ'],list(range(10,81,10)))
            self.assertEqual([struct.unpack('<I',v)[0] for k,v in r['subs'] if k=='FNAM'],[0]*8)
            self.assertEqual(sum(k=='NNAM' for k,v in r['subs']),8)
            self.assertTrue(all(v.endswith(b'\0') for k,v in r['subs'] if k in ('FULL','EDID','NNAM','CNAM')))

if __name__=='__main__':unittest.main(verbosity=2)
