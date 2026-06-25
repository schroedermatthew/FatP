const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
const lines = fs.readFileSync(path, 'utf8').split('\n');
const insert = '<text x="150" y="36" class="shader-stage-title">From the CPU</text>';
for (let i = 0; i < lines.length; i++) {
  if (!lines[i].includes('data-pipeline="p3"')) continue;
  if (lines[i].includes('From the CPU')) break;
  lines[i] = lines[i].replace(
    '<text x="415" y="36" class="shader-stage-title">Per-vertex work</text>',
    insert + '<text x="415" y="36" class="shader-stage-title">Per-vertex work</text>'
  );
  break;
}
fs.writeFileSync(path, lines.join('\n'));
console.log('done');