"""Freeze both runtime collections in native species order for packaging."""
from pathlib import Path
import re,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'Models/Character/PortraitArtwork.hpp').read_text()
files=re.findall(r'"([a-z_]+\.png)"',source)
assert len(files)==47 and len(set(files))==47
assets=[]
for folder in ['Distinct','Company']:
    for name in files:
        p=ROOT/'Resources/Portraits'/folder/name
        assets.append({'file':folder+'/'+name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
target=ROOT/'Resources/Portraits/runtime_manifest.json'
target.write_text(json.dumps({'schema_version':1,'portraits_per_sheet':8,'public_release_authorized':False,'assets':assets},indent=2)+'\n')
print('Recorded 94 runtime sheets / 752 preserved identity slots.')
