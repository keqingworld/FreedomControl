"""Host-compile actual Kernel11/QuestCenter11/KernelSave11 with explicit engine doubles.
Requires real fmt headers (not a fake formatter). JSON value adapter is TEST ONLY.
No Windows/game/real JSON parser validation is implied.
"""
from pathlib import Path
import argparse,shutil,subprocess,tempfile
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--fmt-include',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='fc-kernel-seam-') as tmp:
  t=Path(tmp)
  for f in ('Kernel11.cpp','PlayerFreedom14.cpp','QuestCenter11.cpp','KernelSave11.cpp','CrimeHooks11.hpp','Runtime10.hpp'):shutil.copy2(R/'src'/f,t/f)
  for f in ('PCH.h','json11_adapter.hpp'):shutil.copy2(R/'tests/seam'/f,t/f)
  (t/'Engine.hpp').write_text((R/'src/Engine.hpp').read_text().replace('private:','public:'))
  for f in ('RE/B/BGSScene.h','RE/M/Main.h','RE/M/MenuTopicManager.h','RE/U/UIMessageQueue.h'):
   o=t/f;o.parent.mkdir(parents=True,exist_ok=True);o.write_text('// Explicit test double in PCH.h\n')
  cmd=[a.compiler,'-std=c++23','-Wall','-Wextra','-Wno-missing-field-initializers','-g','-DFC_REAL_FMT11','-DFC_JSON11_ADAPTER','-I'+str(t),'-I'+str(R/'include'),'-I'+a.fmt_include]
  if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  cmd += [str(t/f) for f in ('Kernel11.cpp','PlayerFreedom14.cpp','QuestCenter11.cpp','KernelSave11.cpp')]+[str(R/'src/Core.cpp'),str(R/'tests/seam/kernel_seam.cpp'),'-o',str(t/'test')]
  subprocess.run(cmd,check=True,timeout=60);subprocess.run([str(t/'test')],check=True,timeout=20)
if __name__=='__main__':main()
