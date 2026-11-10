"""Run ONE 10-map mountain-crossing iteration. Explicit review unlocks the next batch.

Uses the production preview/PNG executable, not a substitute renderer. Pillow
only assembles review sheets; production artwork is never modified here.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


TOTAL_MAPS = 100
BATCH_SIZE = 10


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2), encoding='utf-8')
    temporary.replace(path)


def case_spec(index):
    kind = 'world' if index % 2 == 0 else 'region'
    landscapes = ('mountain_pass','highlands','river_valley','lake_country','forest_frontier')
    width,height=((32,24),(40,30),(48,36),(64,48),(80,80))[(index//10)%5]
    return dict(id=f'map-{index:03d}',index=index,kind=kind,seed=914000+index*7919,
                width=width,height=height,template=None,locale='en' if index%3 else 'de',
                landscape=landscapes[(index%10)//2],mountains=(35,55,75)[index%3],
                mountain_pattern=('ranges','mixed','individual')[index%3])


def validate_previous(root, batch):
    if batch == 1:
        return None
    review_path = root / f'batch-{batch-1:03d}' / 'review.json'
    review = json.loads(review_path.read_text(encoding='utf-8'))
    if review.get('batch') != batch - 1 or review.get('engineering_review') != 'accepted' or review.get('open_issues') != []:
        raise RuntimeError('Previous batch has unresolved issues or lacks explicit engineering review.')
    manifest_path = (review_path.parent / review['manifest']).resolve()
    if not manifest_path.is_relative_to(review_path.parent.resolve()) or digest(manifest_path) != review['manifest_sha256']:
        raise RuntimeError('Previous review does not bind its immutable batch manifest.')
    previous = json.loads(manifest_path.read_text(encoding='utf-8'))
    if review.get('remediation'):
        remediation_path = (review_path.parent / review['remediation']).resolve()
        if not remediation_path.is_relative_to(review_path.parent.resolve()) or digest(remediation_path) != review.get('remediation_sha256'):
            raise RuntimeError('Remediation evidence changed after review.')
        if not json.loads(remediation_path.read_text(encoding='utf-8')).get('passed'):
            raise RuntimeError('Reviewed remediation did not pass.')
    if previous['batch'] != batch - 1 or not previous['automated_passed'] or len(previous['cases']) != BATCH_SIZE:
        raise RuntimeError('Previous batch did not complete all ten cases successfully.')
    return dict(path=str(review_path), sha256=digest(review_path))


def ensure_capacity(root, reserve_gb=2):
    # Never start work that is projected to cross the user's 50 GiB floor.
    free = shutil.disk_usage(root).free
    if free < (50 + reserve_gb) * 2**30:
        raise RuntimeError(f'Only {free / 2**30:.1f} GiB free. Clean disposable project outputs, then relocate retained iteration data to F: and rerun this batch; no cases were dropped.')
    return round(free / 2**30, 3)


def run_case(spec, args, directory):
    directory.mkdir()
    output = directory / 'map.json'
    png = directory / 'map.png'
    common = ['--generator-version', '11' if spec['kind'] == 'world' else '12',
              '--recipe-version', str(args.recipe), '--style-id', 'illustrated_atlas',
              '--scope', 'world' if spec['kind'] == 'world' else 'local',
              '--preset', 'tactical' if spec['kind'] == 'tactical' else 'balanced',
              '--width', str(spec['width']), '--height', str(spec['height']),
              '--seed', str(spec['seed']), '--quality-attempts', '12',
              '--pixels-per-cell', '32', '--locale', spec['locale'], '--mountains', str(spec['mountains']), '--landscape', spec['landscape'],
              '--mountain-pattern', spec['mountain_pattern'], '--road-network', 'connected']
    if spec['template']:
        common += ['--tactical-template', spec['template']]
    env = os.environ.copy()
    env.update(TTRPG_DATA_ROOT=str(directory / 'data'), TTRPG_DB_PATH=str(directory / 'data/TTRPG.db'), TTRPG_SETTINGS_PATH=str(directory / 'data/settings'))
    executable = args.build / 'map_generation_preview_export.exe'
    start = time.monotonic()
    record = dict(spec, passed=False, issues=[], arguments=common)
    try:
        for repeated in (False, True):
            destination = directory / 'repeat.json' if repeated else output
            command = [str(executable), '--out', str(destination)] + common
            if not repeated:
                command += ['--png', str(png)]
            with (directory / ('repeat.log' if repeated else 'generation.log')).open('w', encoding='utf-8') as log:
                result = subprocess.run(command, cwd=args.build, env=env, stdout=log, stderr=subprocess.STDOUT,
                                        creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), timeout=180)
            if result.returncode:
                raise RuntimeError(f'Production exporter exited {result.returncode}; inspect generation/repeat log')
        data = json.loads(output.read_text(encoding='utf-8'))
        repeated = json.loads((directory / 'repeat.json').read_text(encoding='utf-8'))
        quality = data['quality']
        record.update(quality=quality, hashes=data['hashes'], png_sha256=digest(png), json_sha256=digest(output))
        if data.get('recipe_version') != args.recipe or repeated.get('recipe_version') != args.recipe:
            record['issues'].append('Actual illustration recipe does not match requested recipe')
        if data['hashes'] != repeated['hashes']:
            record['issues'].append('Same seed/recipe produced different graph, raster or output hashes')
        if not quality['validation_eligible'] or not quality['candidate_floor_passed'] or quality['critical_issue_count']:
            record['issues'].append('Production candidate validation failed')
        if data['width'] != spec['width'] or data['height'] != spec['height'] or not data['render']['compiled_plan_consumed_by_png']:
            record['issues'].append('Dimensions or shared render-plan contract failed')
        record['diagnostics'] = quality['issues']
        record['passed'] = not record['issues']
    except Exception as error:
        record['issues'].append(str(error))
    record['elapsed_seconds'] = round(time.monotonic() - start, 3)
    write_json(directory / 'result.json', record)
    return record


def review_sheets(directory, records):
    from PIL import Image, ImageDraw
    sheets = []
    for offset in range(0, len(records), 10):
        sheet = Image.new('RGB', (2000, 900), '#f4eedf')
        draw = ImageDraw.Draw(sheet)
        for position, record in enumerate(records[offset:offset+10]):
            x, y = (position % 5) * 400, (position // 5) * 450
            png = directory / record['id'] / 'map.png'
            if png.exists():
                with Image.open(png) as source:
                    source.thumbnail((396, 410))
                    sheet.paste(source.convert('RGB'), (x, y))
            draw.text((x+3, y+415), f"{record['id']} {record['kind']} seed {record['seed']}", fill='black')
            draw.text((x+3, y+430), f"{record.get('landscape') or ''} {'OK' if record['passed'] else 'FAILED'}", fill='black')
        path = directory / f'review-sheet-{offset//25+1}.jpg'
        sheet.save(path, quality=94)
        sheets.append(dict(path=path.name, sha256=digest(path)))
    return sheets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--batch', type=int, required=True)
    parser.add_argument('--attempt', type=int, default=1)
    parser.add_argument('--recipe', type=int, default=7)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    args.root, args.build, args.source = args.root.resolve(), args.build.resolve(), args.source.resolve()
    if not 1 <= args.batch <= TOTAL_MAPS // BATCH_SIZE or args.attempt < 1 or args.root.is_relative_to(args.source):
        raise RuntimeError('Use batches 1–10, a positive attempt and evidence outside the checkout.')
    args.root.mkdir(parents=True, exist_ok=True)
    previous = validate_previous(args.root, args.batch)
    free_gb = ensure_capacity(args.root)
    directory = args.root / f'batch-{args.batch:03d}' / f'attempt-{args.attempt:03d}'
    directory.mkdir(parents=True, exist_ok=False)
    corpus = [case_spec(index) for index in range(TOTAL_MAPS)]
    corpus_path = args.root / 'corpus-100.json'
    if corpus_path.exists():
        if json.loads(corpus_path.read_text(encoding='utf-8')) != corpus:
            raise RuntimeError('Frozen corpus changed; do not replace or resample cases.')
    else:
        write_json(corpus_path, corpus)
    commit = subprocess.check_output(['git', '-C', str(args.source), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(args.source), 'status', '--porcelain'], text=True).splitlines()
    assets = args.build/'Resources'
    if not assets.is_dir():
        raise RuntimeError('The built production resource bundle is missing.')
    write_json(directory/'assets.json', [dict(path=str(path.relative_to(assets)), sha256=digest(path))
                                       for path in sorted(assets.rglob('*')) if path.is_file()])
    write_json(directory/'source-hashes.json',[dict(path=str(path.relative_to(args.source)),sha256=digest(path))
        for folder in ('Models/Map','Views/Editor/Widgets/Map/MapCreator','tools')
        for path in sorted((args.source/folder).rglob('*')) if path.is_file() and path.suffix in ('.hpp','.cpp')])
    manifest = dict(source_hashes_sha256=digest(directory/'source-hashes.json'),schema_version=1, batch=args.batch, attempt=args.attempt, corpus_sha256=digest(corpus_path),
                    source=str(args.source), commit=commit, dirty=dirty, recipe=args.recipe,
                    driver_sha256=digest(__file__),
                    assets_sha256=digest(directory/'assets.json'),
                    executable_sha256=digest(args.build / 'map_generation_preview_export.exe'),
                    previous_review=previous, free_gb_before=free_gb, automated_passed=False,
                    engineering_visual_review='pending', human_art_approval='pending', cases=[])
    selected = corpus[(args.batch-1)*BATCH_SIZE:args.batch*BATCH_SIZE]
    # Four in-flight cases maximum. On a failure, finish only those already
    # started; do not advance to another batch or discard the failing seed.
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        pending = {}
        remaining = iter(selected)
        for _ in range(4):
            spec = next(remaining)
            pending[pool.submit(run_case, spec, args, directory/spec['id'])] = spec
        failed = False
        while pending:
            done, _ = concurrent.futures.wait(pending, return_when=concurrent.futures.FIRST_COMPLETED)
            for future in done:
                pending.pop(future)
                record = future.result()
                manifest['cases'].append(record)
                failed |= not record['passed']
                print(f"{record['id']}: {'passed' if record['passed'] else 'FAILED'} ({record['elapsed_seconds']}s)", flush=True)
            write_json(directory/'manifest.json', manifest)
            if not failed:
                ensure_capacity(args.root)
                for _ in range(4-len(pending)):
                    spec = next(remaining, None)
                    if spec is None:
                        break
                    pending[pool.submit(run_case, spec, args, directory/spec['id'])] = spec
    manifest['cases'].sort(key=lambda item: item['index'])
    manifest['review_sheets'] = review_sheets(directory, manifest['cases'])
    manifest['automated_passed'] = len(manifest['cases']) == BATCH_SIZE and all(item['passed'] for item in manifest['cases'])
    manifest['free_gb_after'] = round(shutil.disk_usage(args.root).free/2**30, 3)
    write_json(directory/'manifest.json', manifest)
    print(f"Review required: {directory}", flush=True)
    return 0 if manifest['automated_passed'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
