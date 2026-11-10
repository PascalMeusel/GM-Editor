"""Copy reviewed imagegen output into a new collection without modifying pixels."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'Resources/Portraits/Company'

def main():
    p = argparse.ArgumentParser()
    p.add_argument('species')
    p.add_argument('source', type=Path)
    p.add_argument('--prompt', type=Path, required=True)
    p.add_argument('--origin', default='builtin_image_gen')
    args = p.parse_args()
    species = set(re.findall(r'"([a-z_]+)\.png"',
        (ROOT / 'Models/Character/PortraitArtwork.hpp').read_text(encoding='utf-8')))
    if args.species not in species:
        raise SystemExit('Use an existing species ID, not an arbitrary filename.')
    DEST.mkdir(parents=True, exist_ok=True)
    target = DEST / f'{args.species}.png'
    manifest_path = DEST / 'provenance.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {
        'schema_version': 1, 'collection_id': 1, 'collection': 'Company',
        'state': 'implementation_in_progress', 'generation_tool': 'builtin_image_gen',
        'public_release_authorized': False, 'layout': {'columns': 4, 'rows': 2}, 'entries': []}
    with Image.open(args.source) as art:
        art.verify()
    with Image.open(args.source) as art:
        width, height = art.size
    if width != height or width < 1024:
        raise SystemExit('Expected a square portrait atlas at least 1024 pixels wide.')
    digest = hashlib.sha256(args.source.read_bytes()).hexdigest()
    if target.exists() or any(e['species'] == args.species for e in manifest['entries']):
        raise SystemExit('Species already installed; inspect it before replacing anything.')
    shutil.copy2(args.source, target)
    assert hashlib.sha256(target.read_bytes()).hexdigest() == digest
    manifest['entries'].append({'species': args.species, 'file': target.name,
        'width': width, 'height': height, 'sha256': digest,
        'source_generation': args.source.name, 'origin': args.origin,
        'prompt': args.prompt.read_text(encoding='utf-8'),
        'review': 'Eight-cell composition and individual faces visually inspected; no human or commercial rights acceptance implied.'})
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps({'installed': target.relative_to(ROOT).as_posix(), 'sha256': digest,
                      'collection_sheets': len(manifest['entries'])}))

if __name__ == '__main__':
    main()
