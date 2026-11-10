// Optional headless interoperability check; deliberately not a live Foundry
// scene import and never launches a browser, desktop window, or network call.
// Usage: node tools/uvtt_importer_contract_check.mjs PINNED_DDIMPORT_JS GENERATED_UVTT
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const revision = '3c78eb1b640f00b9bc9b3e30382ebbd03ee374ba';
const sourceSha256 = '0f3332f9fe6d6add85b783aa6d18162e56e3724284879f945a0c2d0163951983';
const [sourcePath, fixturePath] = process.argv.slice(2);
assert(sourcePath && fixturePath, 'Pass pinned importer source and a generated uvtt_export_tests fixture');
const source = readFileSync(sourcePath);
assert.equal(createHash('sha256').update(source).digest('hex'), sourceSha256, 'Unexpected importer source revision/bytes');
const file = JSON.parse(readFileSync(fixturePath, 'utf8'));
assert.equal(file.format, 0.2);
assert.deepEqual(file.resolution, { map_origin: { x: 0, y: 0 }, map_size: { x: 5, y: 4 }, pixels_per_grid: 32 });
assert.equal(file.line_of_sight.length, 5);
assert.equal(file.portals.length, 1);
assert.equal(file.lights.length, 0);
assert.equal(file.gm_editor_export.audience, 'gm_transfer');
assert.equal(file.gm_editor_export.directional_cells_omitted, 1);
assert.equal(file.gm_editor_export.movement_only_cells_omitted, 1);

const errors = [];
class DocumentStub {
  constructor(data) { this.data = data; }
  toObject() { return this.data; }
}
const sandbox = {
  Hooks: { on() {} }, // Registering hooks must not execute application/UI code.
  foundry: { applications: { api: { ApplicationV2: class {}, HandlebarsApplicationMixin: base => base } } },
  WallDocument: DocumentStub,
  AmbientLightDocument: DocumentStub,
  CONST: { WALL_SENSE_TYPES: { NORMAL: 1, PROXIMITY: 4 } },
  game: { settings: { get(module, key) {
    assert.equal(module, 'dd-import'); assert.equal(key, 'openableWindows'); return false;
  } } },
  atob: value => Buffer.from(value, 'base64').toString('latin1'),
  console: { log() {}, error(...values) { errors.push(values.join(' ')); } },
};
vm.createContext(sandbox);
vm.runInContext(source.toString('utf8') + '\nthis.ContractImporter = DDImporter;', sandbox, { timeout: 1000 });
const importer = sandbox.ContractImporter;
const scene = { getDimensions: () => ({ sceneX: 0, sceneY: 0 }) };
const ppg = file.resolution.pixels_per_grid;
const dimensions = importer.prototype._getLargestDimensions([file], ppg);
assert.equal(dimensions.width, 160); assert.equal(dimensions.height, 128);
const walls = importer.GetWalls(file, scene, 0, 0, ppg).map(wall => wall.toObject());
const visibleWalls = walls.filter(wall => wall.c[0] !== wall.c[2] || wall.c[1] !== wall.c[3]);
assert.equal(visibleWalls.length, 5);
const segmentKey = coordinates => Array.from(coordinates).join(',');
const expectedWalls = file.line_of_sight.map(([a, b]) => [a.x*ppg, a.y*ppg, b.x*ppg, b.y*ppg]);
assert.deepEqual(Array.from(visibleWalls, wall => segmentKey(wall.c)).sort(), expectedWalls.map(segmentKey).sort());
const doors = importer.GetDoors(file, scene, 0, ppg).map(door => door.toObject());
assert.equal(doors.length, 1);
assert.deepEqual(Array.from(doors[0].c), [80, 32, 80, 64]);
assert.equal(doors[0].door, 1);
assert.equal(doors[0].sight, 1); assert.equal(doors[0].light, 1);
assert.equal(importer.GetLights(file, scene, ppg).length, 0);
const png = Buffer.from(importer.DecodeImage(file));
assert.equal(importer.getImageType(png.subarray(0, 8).toString('latin1')), 'png');
assert.equal(png.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
assert.equal(png.readUInt32BE(16), 160); assert.equal(png.readUInt32BE(20), 128);
assert.equal(errors.length, 0, errors.join('\n'));
console.log(JSON.stringify({
  importer: 'Moo Man Universal Battlemap Importer', version: '6.1.1', foundryManifestVersion: '14', revision, sourceSha256,
  fixtureSha256: createHash('sha256').update(readFileSync(fixturePath)).digest('hex'),
  gridPixels: ppg, imageWidth: 160, imageHeight: 128,
  wallDocuments: walls.length, nonzeroWallSegments: visibleWalls.length,
  zeroLengthImporterJoins: walls.length - visibleWalls.length, doorDocuments: doors.length,
  scope: 'Actual pinned importer geometry and image decoder with Foundry document stubs; no live scene creation, rendering or upload',
  status: 'passed',
}, null, 2));
