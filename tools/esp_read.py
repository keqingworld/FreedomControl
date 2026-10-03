import struct,zlib,pathlib
class Plugin:
 def __init__(self,path):
  self.path=pathlib.Path(path);self.records=list(self.walk(self.path.read_bytes())); self.masters=[]
  if self.records:
   self.masters=[v.rstrip(b'\0').decode('utf-8','replace') for k,v in self.records[0]['subs'] if k=='MAST']
  self.byid={r['id']:r for r in self.records}
 def key(self,form):
  if not form:return ('',0)
  idx=form>>24; low=form&0xffffff
  if idx<len(self.masters):return self.masters[idx],low
  if idx==len(self.masters):return self.path.name,low
  return f'INVALIDMASTER({idx})',low
 def walk(self,b,start=0,end=None):
  end=len(b) if end is None else end;p=start
  while p<end:
   if p+24>end:raise ValueError(f'truncated header at {p:x}')
   typ=b[p:p+4].decode('ascii');size=struct.unpack_from('<I',b,p+4)[0]
   if typ=='GRUP':
    if size<24 or p+size>end:raise ValueError('bad GRUP')
    yield from self.walk(b,p+24,p+size);p+=size;continue
   flags,fid=struct.unpack_from('<II',b,p+8)
   body=b[p+24:p+24+size]
   if p+24+size>end:raise ValueError('bad record size')
   if flags&0x40000:
    n=struct.unpack_from('<I',body)[0];body=zlib.decompress(body[4:]);assert n==len(body)
   subs=[];s=0;extended=None
   while s<len(body):
    if s+6>len(body):raise ValueError('bad sub header')
    k=body[s:s+4].decode('ascii');n=struct.unpack_from('<H',body,s+4)[0];s+=6
    if k=='XXXX':extended=struct.unpack_from('<I',body,s)[0];s+=n;continue
    if extended is not None:n=extended;extended=None
    v=body[s:s+n];s+=n
    if len(v)!=n:raise ValueError('bad sub length')
    subs.append((k,v))
   edid=next((v.rstrip(b'\0').decode('utf-8','replace') for k,v in subs if k=='EDID'),'')
   yield {'type':typ,'id':fid,'flags':flags,'edid':edid,'subs':subs,'body':body,'offset':p}
   p+=24+size
  assert p==end
 def show(self,r):
  print(r['type'],hex(r['id']),r['edid'], 'flags',hex(r['flags']))
  for k,v in r['subs']:
   val=v.rstrip(b'\0').decode('utf-8','replace') if k in ('EDID','FULL','ALID','ANAM') else v.hex(' ')
   print(k,len(v),val[:512])
if __name__=='__main__':
 import sys
 p=Plugin(sys.argv[1]);print('MASTERS',p.masters)
 for r in p.records:
  if r['type'] in sys.argv[2:]:p.show(r)
