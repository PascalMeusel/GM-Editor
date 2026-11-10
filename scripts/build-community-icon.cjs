// Re-export the project's original vector mark. Requires Node.js and sharp.
// Existing icon candidates are preserved; only CommunityPreviewIcon outputs change.
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');

async function main() {
  const directory = path.resolve(__dirname, '../Resources/AppIcon');
  const input = path.join(directory, 'CommunityPreviewIcon.svg');
  await sharp(input).resize(512, 512).png().toFile(path.join(directory, 'CommunityPreviewIcon.png'));
  const sizes = [16, 24, 32, 48, 64, 128, 256];
  const images = await Promise.all(sizes.map(size => sharp(input).resize(size, size).png().toBuffer()));
  const header = Buffer.alloc(6 + sizes.length * 16);
  header.writeUInt16LE(1, 2);
  header.writeUInt16LE(sizes.length, 4);
  let offset = header.length;
  images.forEach((image, i) => {
    const row = 6 + i * 16;
    header[row] = sizes[i] === 256 ? 0 : sizes[i];
    header[row + 1] = header[row];
    header.writeUInt16LE(1, row + 4);
    header.writeUInt16LE(32, row + 6);
    header.writeUInt32LE(image.length, row + 8);
    header.writeUInt32LE(offset, row + 12);
    offset += image.length;
  });
  fs.writeFileSync(path.join(directory, 'CommunityPreviewIcon.ico'), Buffer.concat([header, ...images]));
}
main().catch(error => { console.error(error.message); process.exitCode = 1; });
